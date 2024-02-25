// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/zucchini/heuristic_ensemble_matcher.h"

#include <algorithm>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "base/functional/bind.h"
#include "base/logging.h"
#include "base/memory/raw_ref.h"
#include "base/numerics/safe_conversions.h"
#include "base/strings/stringprintf.h"
#include "components/zucchini/binary_data_histogram.h"
#include "components/zucchini/element_detection.h"
#include "components/zucchini/image_utils.h"
#include "components/zucchini/io_utils.h"

namespace zucchini {

namespace {

/******** Helper Functions ********/

// Uses |detector| to find embedded executables inside |image|, and returns the
// result on success, or std::nullopt on failure,  which occurs if too many (>
// |kElementLimit|) elements are found.
std::optional<std::vector<Element>> FindEmbeddedElements(
    ConstBufferView image,
    const std::string& name,
    ElementDetector&& detector) {
  // Maximum number of Elements in a file. This is enforced because our matching
  // algorithm is O(n^2), which suffices for regular archive files that should
  // have up to 10's of executable files. An archive containing 100's of
  // executables is likely pathological, and is rejected to prevent exploits.
  static constexpr size_t kElementLimit = 256;
  std::vector<Element> elements;
  ElementFinder element_finder(image, std::move(detector));
  for (auto element = element_finder.GetNext();
       element.has_value() && elements.size() <= kElementLimit;
       element = element_finder.GetNext()) {
    elements.push_back(*element);
  }
  if (elements.size() >= kElementLimit) {
    LOG(WARNING) << name << ": Found too many elements.";
    return std::nullopt;
  }
  LOG(INFO) << name << ": Found " << elements.size() << " elements.";
  return elements;
}

// Determines whether a proposed comparison between Elements should be rejected
// early, to decrease the likelihood of creating false-positive matches, which
// may be costly for patching. Our heuristic simply prohibits big difference in
// size (relative and absolute) between matched elements.
bool UnsafeDifference(const Element& old_element, const Element& new_element) {
  static constexpr double kMaxBloat = 2.0;
  static constexpr size_t kMinWorrysomeDifference = 2 << 20;  // 2MB
  size_t lo_size = std::min(old_element.size, new_element.size);
  size_t hi_size = std::max(old_element.size, new_element.size);
  if (hi_size - lo_size < kMinWorrysomeDifference)
    return false;
  if (hi_size < lo_size * kMaxBloat)
    return false;
  return true;
}

std::ostream& operator<<(std::ostream& stream, const Element& elt) {
  stream << "(" << CastExecutableTypeToString(elt.exe_type) << ", "
         << AsHex<8, size_t>(elt.offset) << " +" << AsHex<8, size_t>(elt.size)
         << ")";
  return stream;
}

/******** MatchingInfoOut ********/

// A class to output detailed information during ensemble matching. Extracting
// the functionality to a separate class decouples formatting and printing logic
// from matching logic. The base class consists of stubs.
class MatchingInfoOut {
 protected:
  MatchingInfoOut() = default;
  MatchingInfoOut(const MatchingInfoOut&) = delete;
  const MatchingInfoOut& operator=(const MatchingInfoOut&) = delete;

 public:
  virtual ~MatchingInfoOut() = default;
  virtual void InitSizes(size_t old_size, size_t new_size) {}
  virtual void DeclareTypeMismatch(int iold, int inew) {}
  virtual void DeclareUnsafeDistance(int iold, int inew) {}
  virtual void DeclareCandidate(int iold, int inew) {}
  virtual void DeclareMatch(int iold,
                            int inew,
                            double dist,
                            bool is_identical) {}
  virtual void DeclareOutlier(int iold, int inew) {}

  virtual void OutputCompare(const Element& old_element,
                             const Element& new_element,
                             double dist) {}

  virtual void OutputMatch(const Element& best_old_element,
                           const Element& new_element,
                           bool is_identical,
                           double best_dist) {}

  virtual void OutputScores(const std::string& stats) {}

  virtual void OutputTextGrid() {}
};

/******** MatchingInfoTerse ********/

// A terse MatchingInfoOut that prints only basic information, using LOG().
class MatchingInfoOutTerse : public MatchingInfoOut {
 public:
  MatchingInfoOutTerse() = default;
  MatchingInfoOutTerse(const MatchingInfoOutTerse&) = delete;
  const MatchingInfoOutTerse& operator=(const MatchingInfoOutTerse&) = delete;
  ~MatchingInfoOutTerse() override = default;

  void OutputScores(const std::string& stats) override {
    LOG(INFO) << "Best dists: " << stats;
  }
};

/******** MatchingInfoOutVerbose ********/

// A verbose MatchingInfoOut that prints detailed information using |out_|,
// including comparison pairs, scores, and a text grid representation of
// pairwise matching results.
class MatchingInfoOutVerbose : public MatchingInfoOut {
 public:
  explicit MatchingInfoOutVerbose(std::ostream& out) : out_(out) {}
  MatchingInfoOutVerbose(const MatchingInfoOutVerbose&) = delete;
  const MatchingInfoOutVerbose& operator=(const MatchingInfoOutVerbose&) =
      delete;
  ~MatchingInfoOutVerbose() override = default;

  // Outputs sizes and initializes |text_grid_|.
  void InitSizes(size_t old_size, size_t new_size) override {
    *out_ << "Comparing old (" << old_size << " elements) and new (" << new_size
          << " elements)" << std::endl;
    text_grid_.assign(new_size, std::string(old_size, '-'));
    best_dist_.assign(new_size, -1.0);
  }

  // Functions to update match status in text grid representation.

  void DeclareTypeMismatch(int iold, int inew) override {
    text_grid_[inew][iold] = 'T';
  }
  void DeclareUnsafeDistance(int iold, int inew) override {
    text_grid_[inew][iold] = 'U';
  }
  void DeclareCandidate(int iold, int inew) override {
    text_grid_[inew][iold] = 'C';  // Provisional.
  }
  void DeclareMatch(int iold,
                    int inew,
                    double dist,
                    bool is_identical) override {
    text_grid_[inew][iold] = is_identical ? 'I' : 'M';
    best_dist_[inew] = dist;
  }
  void DeclareOutlier(int iold, int inew) override {
    text_grid_[inew][iold] = 'O';
  }

  // Functions to print detailed information.

  void OutputCompare(const Element& old_element,
                     const Element& new_element,
                     double dist) override {
    *out_ << "Compare old" << old_element << " to new" << new_element << " --> "
          << base::StringPrintf("%.5f", dist) << std::endl;
  }

  void OutputMatch(const Element& best_old_element,
                   const Element& new_element,
                   bool is_identical,
                   double best_dist) override {
    if (is_identical) {
      *out_ << "Skipped old" << best_old_element << " - identical to new"
            << new_element;
    } else {
      *out_ << "Matched old" << best_old_element << " to new" << new_element
            << " --> " << base::StringPrintf("%.5f", best_dist);
    }
    *out_ << std::endl;
  }

  void OutputScores(const std::string& stats) override {
    *out_ << "Best dists: " << stats << std::endl;
  }

  void OutputTextGrid() override {
    int new_size = static_cast<int>(text_grid_.size());
    for (int inew = 0; inew < new_size; ++inew) {
      const std::string& line = text_grid_[inew];
      *out_ << "  ";
      for (char ch : line) {
        char prefix = (ch == 'I' || ch == 'M') ? '(' : ' ';
        char suffix = (ch == 'I' || ch == 'M') ? ')' : ' ';
        *out_ << prefix << ch << suffix;
      }
      if (best_dist_[inew] >= 0)
        *out_ << "   " << base::StringPrintf("%.5f", best_dist_[inew]);
      *out_ << std::endl;
    }
    if (!text_grid_.empty()) {
      *out_ << "  Legend: I = identical, M = matched, T = type mismatch, "
               "U = unsafe distance, C = candidate, O = outlier, - = skipped."
            << std::endl;
    }
  }

 private:
  const raw_ref<std::ostream> out_;

  // Text grid representation of matches. Rows correspond to "old" and columns
  // correspond to "new".
  std::vector<std::string> text_grid_;

  // For each "new" element, distance of best match. -1 denotes no match.
  std::vector<double> best_dist_;
};

}  // namespace

/******** HeuristicEnsembleMatcher ********/

HeuristicEnsembleMatcher::HeuristicEnsembleMatcher(std::ostream* out)
    : out_(out) {}

HeuristicEnsembleMatcher::~HeuristicEnsembleMatcher() = default;

bool HeuristicEnsembleMatcher::RunMatch(ConstBufferView old_image,
                                        ConstBufferView new_image) {
  DCHECK(matches_.empty());
  LOG(INFO) << "Start matching.";

  // Find all elements in "old" and "new".
  std::optional<std::vector<Element>> old_elements =
      FindEmbeddedElements(old_image, "Old file",
                           base::BindRepeating(DetectElementFromDisassembler));
  if (!old_elements.has_value())
    return false;
  std::optional<std::vector<Element>> new_elements =
      FindEmbeddedElements(new_image, "New file",
                           base::BindRepeating(DetectElementFromDisassembler));
  if (!new_elements.has_value())
    return false;

  std::unique_ptr<MatchingInfoOut> info_out;
  if (out_)
    info_out = std::make_unique<MatchingInfoOutVerbose>(*out_);
  else
    info_out = std::make_unique<MatchingInfoOutTerse>();

  const int num_new_elements = base::checked_cast<int>(new_elements->size());
  const int num_old_elements = base::checked_cast<int>(old_elements->size());
  info_out->InitSizes(num_old_elements, num_new_elements);

  // For each "new" element, match it with the "old" element that's nearest to
  // it, with distance determined by BinaryDataHistogram. The resulting
  // "old"-"new" pairs are stored into |results|. Possibilities:
  // - Type mismatch: No match.
  // - UnsafeDifference() heuristics fail: No match.
  // - Identical match: Skip "new" since this is a trivial case.
  // - Non-identical match: Match "new" with "old" with min distance.
  // - No match: Skip "new".
  struct Results {
    int iold;
    int inew;
    double dist;
  };
  std::vector<Results> results;

  // Precompute histograms for "old" since they get reused.
  std::vector<BinaryDataHistogram> old_his(num_old_elements);
  for (int iold = 0; iold < num_old_elements; ++iold) {
    ConstBufferView sub_image(old_image[(*old_elements)[iold]]);
    old_his[iold].Compute(sub_image);
    // ProgramDetector should have imposed minimal size limit to |sub_image|.
    // Therefore resulting histogram are expected to be valid.
    CHECK(old_his[iold].IsValid());
  }

  const int kUninitIold = num_old_elements;
  for (int inew = 0; inew < num_new_elements; ++inew) {
    const Element& cur_new_element = (*new_elements)[inew];
    ConstBufferView cur_new_sub_image(new_image[cur_new_element.region()]);
    BinaryDataHistogram new_his;
    new_his.Compute(cur_new_sub_image);
    CHECK(new_his.IsValid());

    double best_dist = HUGE_VAL;
    int best_iold = kUninitIold;
    bool is_identical = false;

    for (int iold = 0; iold < num_old_elements; ++iold) {
      const Element& cur_old_element = (*old_elements)[iold];
      if (cur_old_element.exe_type != cur_new_element.exe_type) {
        info_out->DeclareTypeMismatch(iold, inew);
        continue;
      }
      if (UnsafeDifference(cur_old_element, cur_new_element)) {
        info_out->DeclareUnsafeDistance(iold, inew);
        continue;
      }
      double dist = old_his[iold].Distance(new_his);
      info_out->DeclareCandidate(iold, inew);
      info_out->OutputCompare(cur_old_element, cur_new_element, dist);
      if (best_dist > dist) {  // Tie resolution: First-one, first-serve.
        best_iold = iold;
        best_dist = dist;
        if (best_dist == 0) {
          ConstBufferView sub_image(old_image[cur_old_element.region()]);
          if (sub_image.equals(cur_new_sub_image)) {
            is_identical = true;
            break;
          }
        }
      }
    }

    if (best_iold != kUninitIold) {
      const Element& best_old_element = (*old_elements)[best_iold];
      info_out->DeclareMatch(best_iold, inew, best_dist, is_identical);
      if (is_identical)  // Skip "new" if identical match is found.
        ++num_identical_;
      else
        results.push_back({best_iold, inew, best_dist});
      info_out->OutputMatch(best_old_element, cur_new_element, is_identical,
                            best_dist);
    }
  }

  // Populate |matches_| from |result|. To reduce that chance of false-positive
  // matches, statistics on dists are computed. If a match's |dist| is an
  // outlier then it is rejected.
  if (results.size() > 0) {
    OutlierDetector detector;
    for (const auto& result : results) {
      if (result.dist > 0)
        detector.Add(result.dist);
    }
    detector.Prepare();
    info_out->OutputScores(detector.RenderStats());
    for (const Results& result : results) {
      if (detector.DecideOutlier(result.dist) > 0) {
        info_out->DeclareOutlier(result.iold, result.inew);
      } else {
        matches_.push_back(
            {(*old_elements)[result.iold], (*new_elements)[result.inew]});
      }
    }
    info_out->OutputTextGrid();
  }

  Trim();
  return true;
}

}  // namespace zucchini
