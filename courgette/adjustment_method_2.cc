// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "courgette/adjustment_method.h"

#include <stddef.h>
#include <stdint.h>

#include <algorithm>
#include <limits>
#include <list>
#include <map>
#include <set>
#include <string>
#include <vector>

#include "base/format_macros.h"
#include "base/logging.h"
#include "base/macros.h"
#include "base/strings/stringprintf.h"
#include "base/time/time.h"
#include "courgette/assembly_program.h"
#include "courgette/courgette.h"
#include "courgette/encoded_program.h"

/*

Shingle weighting matching.

We have a sequence S1 of symbols from alphabet A1={A,B,C,...} called the 'model'
and a second sequence of S2 of symbols from alphabet A2={U,V,W,....} called the
'program'.  Each symbol in A1 has a unique numerical name or index.  We can
transcribe the sequence S1 to a sequence T1 of indexes of the symbols. We wish
to assign indexes to the symbols in A2 so that when we transcribe S2 into T2, T2
has long subsequences that occur in T1.  This will ensure that the sequence
T1;T2 compresses to be only slightly larger than the compressed T1.

The algorithm for matching members of S2 with members of S1 is eager - it makes
matches without backtracking, until no more matches can be made.  Each variable
(symbol) U,V,... in A2 has a set of candidates from A1, each candidate with a
weight summarizing the evidence for the match.  We keep a VariableQueue of
U,V,... sorted by how much the evidence for the best choice outweighs the
evidence for the second choice, i.e. prioritized by how 'clear cut' the best
assignment is.  We pick the variable with the most clear-cut candidate, make the
assignment, adjust the evidence and repeat.

What has not been described so far is how the evidence is gathered and
maintained.  We are working under the assumption that S1 and S2 are largely
similar.  (A different assumption might be that S1 and S2 are dissimilar except
for many long subsequences.)

A naive algorithm would consider all pairs (A,U) and for each pair assess the
benefit, or score, the assignment U:=A.  The score might count the number of
occurrences of U in S2 which appear in similar contexts to A in S1.

To distinguish contexts we view S1 and S2 as a sequence of overlapping k-length
substrings or 'shingles'.  Two shingles are compatible if the symbols in one
shingle could be matched with the symbols in the other symbol.  For example, ABC
is *not* compatible with UVU because it would require conflicting matches A=U
and C=U.  ABC is compatible with UVW, UWV, WUV, VUW etc.  We can't tell which
until we make an assignment - the compatible shingles form an equivalence class.
After assigning U:=A then only UVW and UWV (equivalently AVW, AWV) are
compatible.  As we make assignments the number of equivalence classes of
shingles increases and the number of members of each equivalence class
decreases.  The compatibility test becomes more restrictive.

We gather evidence for the potential assignment U:=A by counting how many
shingles containing U are compatible with shingles containing A.  Thus symbols
occurring a large number of times in compatible contexts will be assigned first.

Finding the 'most clear-cut' assignment by considering all pairs symbols and for
each pair comparing the contexts of each pair of occurrences of the symbols is
computationally infeasible.  We get the job done in a reasonable time by
approaching it 'backwards' and making incremental changes as we make
assignments.

First the shingles are partitioned according to compatibility.  In S1=ABCDD and
S2=UVWXX we have a total of 6 shingles, each occuring once. (ABC:1 BCD:1 CDD:1;
UVW:1 VWX: WXX:1) all fit the pattern <V0 V1 V2> or the pattern <V0 V1 V1>.  The
first pattern indicates that each position matches a different symbol, the
second pattern indicates that the second symbol is repeated.

  pattern      S1 members      S2 members
  <V0 V1 V2>:  {ABC:1, BCD:1}; {UVW:1, VWX:1}
  <V0 V1 V1>:  {CDD:1}         {WXX:1}

The second pattern appears to have a unique assignment but we don't make the
assignment on such scant evidence.  If S1 and S2 do not match exactly, there
will be numerous spurious low-score matches like this.  Instead we must see what
assignments are indicated by considering all of the evidence.

First pattern has 2 x 2 = 4 shingle pairs.  For each pair we count the number
of symbol assignments.  For ABC:a * UVW:b accumulate min(a,b) to each of
  {U:=A, V:=B, W:=C}.
After accumulating over all 2 x 2 pairs:
  U: {A:1  B:1}
  V: {A:1  B:2  C:1}
  W: {B:1  C:2  D:1 }
  X: {C:1  D:1}
The second pattern contributes:
  W: {C:1}
  X: {D:2}
Sum:
  U: {A:1  B:1}
  V: {A:1  B:2  C:1}
  W: {B:1  C:3  D:1}
  X: {C:1  D:3}

From this we decide to assign X:=D (because this assignment has both the largest
difference above the next candidate (X:=C) and this is also the largest
proportionately over the sum of alternatives).

Lets assume D has numerical 'name' 77.  The assignment X:=D sets X to 77 too.
Next we repartition all the shingles containing X or D:

  pattern      S1 members      S2 members
  <V0 V1 V2>:  {ABC:1};        {UVW:1}
  <V0 V1 77>:  {BCD:1};        {VWX:1}
  <V0 77 77>:  {CDD:1}         {WXX:1}
As we repartition, we recalculate the contributions to the scores:
  U: {A:1}
  V: {B:2}
  W: {C:3}
All the remaining assignments are now fixed.

There is one step in the incremental algorithm that is still infeasibly
expensive: the contributions due to the cross product of large equivalence
classes.  We settle for making an approximation by computing the contribution of
the cross product of only the most common shingles.  The hope is that the noise
from the long tail of uncounted shingles is well below the scores being used to
pick assignments.  The second hope is that as assignment are made, the large
equivalence class will be partitioned into smaller equivalence classes, reducing
the noise over time.

In the code below the shingles are bigger (Shingle::kWidth = 5).
Class ShinglePattern holds the data for one pattern.

There is an optimization for this case:
  <V0 V1 V1>:  {CDD:1}         {WXX:1}

Above we said that we don't make an assignment on this "scant evidence".  There
is an exception: if there is only one variable unassigned (more like the <V0 77
77> pattern) AND there are no occurrences of C and W other than those counted in
this pattern, then there is no competing evidence and we go ahead with the
assignment immediately.  This produces slightly better results because these
cases tend to be low-scoring and susceptible to small mistakes made in
low-scoring assignments in the approximation for large equivalence classes.

*/

namespace courgette {
namespace adjustment_method_2 {

////////////////////////////////////////////////////////////////////////////////

class AssignmentCandidates;
class LabelInfoMaker;
class Shingle;
class ShinglePattern;

// The purpose of adjustment is to assign indexes to Labels of a program 'p' to
// make the sequence of indexes similar to a 'model' program 'm'.  Labels
// themselves don't have enough information to do this job, so we work with a
// LabelInfo surrogate for each label.
//
class LabelInfo {
 public:
  // Just a no-argument constructor and copy constructor.  Actual LabelInfo
  // objects are allocated in std::pair structs in a std::map.
  LabelInfo()
      : label_(nullptr),
        is_model_(false),
        debug_index_(0),
        refs_(0),
        assignment_(nullptr),
        candidates_(nullptr) {}

  ~LabelInfo();

  AssignmentCandidates* candidates();

  Label* label_;             // The label that this info a surrogate for.

  uint32_t is_model_ : 1;      // Is the label in the model?
  uint32_t debug_index_ : 31;  // A small number for naming the label in debug
                               // output. The pair (is_model_, debug_index_) is
                               // unique.

  int refs_;                 // Number of times this Label is referenced.

  LabelInfo* assignment_;    // Label from other program corresponding to this.

  std::vector<uint32_t> positions_;  // Offsets into the trace of references.

 private:
  AssignmentCandidates* candidates_;

  void operator=(const LabelInfo*);  // Disallow assignment only.
  // Public compiler generated copy constructor is needed to constuct
  // std::pair<Label*, LabelInfo> so that fresh LabelInfos can be allocated
  // inside a std::map.
};

typedef std::vector<LabelInfo*> Trace;

std::string ToString(const LabelInfo* info) {
  std::string s;
  base::StringAppendF(&s, "%c%d", "pm"[info->is_model_], info->debug_index_);
  if (info->label_->index_ != Label::kNoIndex)
    base::StringAppendF(&s, " (%d)", info->label_->index_);

  base::StringAppendF(&s, " #%u", info->refs_);
  return s;
}

// LabelInfoMaker maps labels to their surrogate LabelInfo objects.
class LabelInfoMaker {
 public:
  LabelInfoMaker() : debug_label_index_gen_(0) {}

  LabelInfo* MakeLabelInfo(Label* label, bool is_model, uint32_t position) {
    LabelInfo& slot = label_infos_[label];
    if (slot.label_ == nullptr) {
      slot.label_ = label;
      slot.is_model_ = is_model;
      slot.debug_index_ = ++debug_label_index_gen_;
    }
    slot.positions_.push_back(position);
    ++slot.refs_;
    return &slot;
  }

  void ResetDebugLabel() { debug_label_index_gen_ = 0; }

 private:
  int debug_label_index_gen_;

  // Note LabelInfo is allocated 'flat' inside map::value_type, so the LabelInfo
  // lifetimes are managed by the map.
  std::map<Label*, LabelInfo> label_infos_;

  DISALLOW_COPY_AND_ASSIGN(LabelInfoMaker);
};

struct OrderLabelInfo {
  bool operator()(const LabelInfo* a, const LabelInfo* b) const {
    if (a->label_->rva_ < b->label_->rva_) return true;
    if (a->label_->rva_ > b->label_->rva_) return false;
    if (a == b) return false;
    return a->positions_ < b->positions_;  // Lexicographic ordering of vector.
  }
};

// AssignmentCandidates is a priority queue of candidate assignments to
// a single program LabelInfo, |program_info_|.
class AssignmentCandidates {
 public:
  explicit AssignmentCandidates(LabelInfo* program_info)
      : program_info_(program_info) {}

  LabelInfo* program_info() const { return program_info_; }

  bool empty() const { return label_to_score_.empty(); }

  LabelInfo* top_candidate() const { return queue_.begin()->second; }

  void Update(LabelInfo* model_info, int delta_score) {
    LOG_ASSERT(delta_score != 0);
    int old_score = 0;
    int new_score = 0;
    LabelToScore::iterator p = label_to_score_.find(model_info);
    if (p != label_to_score_.end()) {
      old_score = p->second;
      new_score = old_score + delta_score;
      queue_.erase(ScoreAndLabel(old_score, p->first));
      if (new_score == 0) {
        label_to_score_.erase(p);
      } else {
        p->second = new_score;
        queue_.insert(ScoreAndLabel(new_score, model_info));
      }
    } else {
      new_score = delta_score;
      label_to_score_.insert(std::make_pair(model_info, new_score));
      queue_.insert(ScoreAndLabel(new_score, model_info));
    }
    LOG_ASSERT(queue_.size() == label_to_score_.size());
  }

  int TopScore() const {
    int first_value = 0;
    int second_value = 0;
    Queue::const_iterator p = queue_.begin();
    if (p != queue_.end()) {
      first_value = p->first;
      ++p;
      if (p != queue_.end()) {
        second_value = p->first;
      }
    }
    return first_value - second_value;
  }

  bool HasPendingUpdates() { return !pending_updates_.empty(); }

  void AddPendingUpdate(LabelInfo* model_info, int delta_score) {
    LOG_ASSERT(delta_score != 0);
    pending_updates_[model_info] += delta_score;
  }

  void ApplyPendingUpdates() {
    // TODO(sra): try to walk |pending_updates_| and |label_to_score_| in
    // lockstep.  Try to batch updates to |queue_|.
    size_t zeroes = 0;
    for (LabelToScore::iterator p = pending_updates_.begin();
         p != pending_updates_.end();
         ++p) {
      if (p->second != 0)
        Update(p->first, p->second);
      else
        ++zeroes;
    }
    pending_updates_.clear();
  }

  void Print(int max) {
    VLOG(2) << "score "  << TopScore() << "  " << ToString(program_info_)
            << " := ?";
    if (!pending_updates_.empty())
      VLOG(2) << pending_updates_.size() << " pending";
    int count = 0;
    for (Queue::iterator q = queue_.begin();  q != queue_.end();  ++q) {
      if (++count > max) break;
      VLOG(2) << "   " << q->first << "  " << ToString(q->second);
    }
  }

 private:
  typedef std::map<LabelInfo*, int, OrderLabelInfo> LabelToScore;
  typedef std::pair<int, LabelInfo*> ScoreAndLabel;
  struct OrderScoreAndLabelByScoreDecreasing {
    OrderLabelInfo tie_breaker;
    bool operator()(const ScoreAndLabel& a, const ScoreAndLabel& b) const {
      if (a.first > b.first) return true;
      if (a.first < b.first) return false;
      return tie_breaker(a.second, b.second);
    }
  };
  typedef std::set<ScoreAndLabel, OrderScoreAndLabelByScoreDecreasing> Queue;

  LabelInfo* program_info_;
  LabelToScore label_to_score_;
  LabelToScore pending_updates_;
  Queue queue_;
};

AssignmentCandidates* LabelInfo::candidates() {
  if (candidates_ == nullptr)
    candidates_ = new AssignmentCandidates(this);
  return candidates_;
}

LabelInfo::~LabelInfo() {
  delete candidates_;
}

// A Shingle is a short fixed-length string of LabelInfos that actually occurs
// in a Trace.  A Shingle may occur many times.  We repesent the Shingle by the
// position of one of the occurrences in the Trace.
class Shingle {
 public:
  static const uint8_t kWidth = 5;

  struct InterningLess {
    bool operator()(const Shingle& a, const Shingle& b) const;
  };

  typedef std::set<Shingle, InterningLess> OwningSet;

  static Shingle* Find(const Trace& trace, size_t position,
                       OwningSet* owning_set) {
    std::pair<OwningSet::iterator, bool> pair =
        owning_set->insert(Shingle(trace, position));
    // pair.first iterator 'points' to the newly inserted Shingle or the
    // previouly inserted one that looks the same according to the comparator.

    // const_cast required because key is const.  We modify the Shingle
    // extensively but not in a way that affects InterningLess.
    Shingle* shingle = const_cast<Shingle*>(&*pair.first);
    shingle->add_position(position);
    return shingle;
  }

  LabelInfo* at(size_t i) const { return trace_[exemplar_position_ + i]; }
  void add_position(size_t position) {
    positions_.push_back(static_cast<uint32_t>(position));
  }
  int position_count() const { return static_cast<int>(positions_.size()); }

  bool InModel() const { return at(0)->is_model_; }

  ShinglePattern* pattern() const { return pattern_; }
  void set_pattern(ShinglePattern* pattern) { pattern_ = pattern; }

  struct PointerLess {
    bool operator()(const Shingle* a, const Shingle* b) const {
      // Arbitrary but repeatable (memory-address) independent ordering:
      return a->exemplar_position_ < b->exemplar_position_;
      // return InterningLess()(*a, *b);
    }
  };

 private:
  Shingle(const Trace& trace, size_t exemplar_position)
      : trace_(trace),
        exemplar_position_(exemplar_position),
        pattern_(nullptr) {}

  const Trace& trace_;             // The shingle lives inside trace_.
  size_t exemplar_position_;       // At this position (and other positions).
  std::vector<uint32_t> positions_;  // Includes exemplar_position_.

  ShinglePattern* pattern_;       // Pattern changes as LabelInfos are assigned.

  friend std::string ToString(const Shingle* instance);

  // We can't disallow the copy constructor because we use std::set<Shingle> and
  // VS2005's implementation of std::set<T>::set() requires T to have a copy
  // constructor.
  //   DISALLOW_COPY_AND_ASSIGN(Shingle);
  void operator=(const Shingle&) = delete;  // Disallow assignment only.
};

std::string ToString(const Shingle* instance) {
  std::string s;
  const char* sep = "<";
  for (uint8_t i = 0; i < Shingle::kWidth; ++i) {
    // base::StringAppendF(&s, "%s%x ", sep, instance.at(i)->label_->rva_);
    s += sep;
    s += ToString(instance->at(i));
    sep = ", ";
  }
  base::StringAppendF(&s, ">(%" PRIuS ")@{%d}",
                      instance->exemplar_position_,
                      instance->position_count());
  return s;
}


bool Shingle::InterningLess::operator()(
    const Shingle& a,
    const Shingle& b) const {
  for (uint8_t i = 0; i < kWidth; ++i) {
    LabelInfo* info_a = a.at(i);
    LabelInfo* info_b = b.at(i);
    if (info_a->label_->rva_ < info_b->label_->rva_)
      return true;
    if (info_a->label_->rva_ > info_b->label_->rva_)
      return false;
    if (info_a->is_model_ < info_b->is_model_)
      return true;
    if (info_a->is_model_ > info_b->is_model_)
      return false;
    if (info_a != info_b) {
      NOTREACHED();
    }
  }
  return false;
}

class ShinglePattern {
 public:
  enum { kOffsetMask = 7,  // Offset lives in low bits.
         kFixed    = 0,    // kind & kVariable == 0  => fixed.
         kVariable = 8     // kind & kVariable == 1  => variable.
         };
  // sequence[position + (kinds_[i] & kOffsetMask)] gives LabelInfo for position
  // i of shingle.  Below, second 'A' is duplicate of position 1, second '102'
  // is duplicate of position 0.
  //
  //   <102, A, 103, A , 102>
  //      --> <kFixed+0, kVariable+1, kFixed+2, kVariable+1, kFixed+0>
  struct Index {
    explicit Index(const Shingle* instance);
    uint8_t kinds_[Shingle::kWidth];
    uint8_t variables_;
    uint8_t unique_variables_;
    uint8_t first_variable_index_;
    uint32_t hash_;
    int assigned_indexes_[Shingle::kWidth];
  };

  // ShinglePattern keeps histograms of member Shingle instances, ordered by
  // decreasing number of occurrences.  We don't have a pair (occurrence count,
  // Shingle instance), so we use a FreqView adapter to make the instance
  // pointer look like the pair.
  class FreqView {
   public:
    explicit FreqView(const Shingle* instance) : instance_(instance) {}
    int count() const { return instance_->position_count(); }
    const Shingle* instance() const { return instance_; }
    struct Greater {
      bool operator()(const FreqView& a, const FreqView& b) const {
        if (a.count() > b.count()) return true;
        if (a.count() < b.count()) return false;
        return resolve_ties(a.instance(), b.instance());
      }
     private:
      Shingle::PointerLess resolve_ties;
    };
   private:
    const Shingle* instance_;
  };

  typedef std::set<FreqView, FreqView::Greater> Histogram;

  ShinglePattern()
      : index_(nullptr), model_coverage_(0), program_coverage_(0) {}

  const Index* index_;  // Points to the key in the owning map value_type.
  Histogram model_histogram_;
  Histogram program_histogram_;
  int model_coverage_;
  int program_coverage_;
};

std::string ToString(const ShinglePattern::Index* index) {
  std::string s;
  if (index == nullptr) {
    s = "<null>";
  } else {
    base::StringAppendF(&s, "<%d: ", index->variables_);
    const char* sep = "";
    for (uint8_t i = 0; i < Shingle::kWidth; ++i) {
      s += sep;
      sep = ", ";
      uint32_t kind = index->kinds_[i];
      int offset = kind & ShinglePattern::kOffsetMask;
      if (kind & ShinglePattern::kVariable)
        base::StringAppendF(&s, "V%d", offset);
      else
        base::StringAppendF(&s, "%d", index->assigned_indexes_[offset]);
     }
    base::StringAppendF(&s, " %x", index->hash_);
    s += ">";
  }
  return s;
}

std::string HistogramToString(const ShinglePattern::Histogram& histogram,
                              size_t snippet_max) {
  std::string s;
  size_t histogram_size = histogram.size();
  size_t snippet_size = 0;
  for (ShinglePattern::Histogram::const_iterator p = histogram.begin();
       p != histogram.end();
       ++p) {
    if (++snippet_size > snippet_max && snippet_size != histogram_size) {
      s += " ...";
      break;
    }
    base::StringAppendF(&s, " %d", p->count());
  }
  return s;
}

std::string HistogramToStringFull(const ShinglePattern::Histogram& histogram,
                                  const char* indent,
                                  size_t snippet_max) {
  std::string s;

  size_t histogram_size = histogram.size();
  size_t snippet_size = 0;
  for (ShinglePattern::Histogram::const_iterator p = histogram.begin();
       p != histogram.end();
       ++p) {
    s += indent;
    if (++snippet_size > snippet_max && snippet_size != histogram_size) {
      s += "...\n";
      break;
    }
    base::StringAppendF(&s, "(%d) ", p->count());
    s += ToString(&(*p->instance()));
    s += "\n";
  }
  return s;
}

std::string ToString(const ShinglePattern* pattern, size_t snippet_max = 3) {
  std::string s;
  if (pattern == nullptr) {
    s = "<null>";
  } else {
    s = "{";
    s += ToString(pattern->index_);
    base::StringAppendF(&s, ";  %d(%d):",
                        static_cast<int>(pattern->model_histogram_.size()),
                        pattern->model_coverage_);

    s += HistogramToString(pattern->model_histogram_, snippet_max);
    base::StringAppendF(&s, ";  %d(%d):",
                        static_cast<int>(pattern->program_histogram_.size()),
                        pattern->program_coverage_);
    s += HistogramToString(pattern->program_histogram_, snippet_max);
    s += "}";
  }
  return s;
}

std::string ShinglePatternToStringFull(const ShinglePattern* pattern,
                                       size_t max) {
  std::string s;
  s += ToString(pattern->index_);
  s += "\n";
  size_t model_size = pattern->model_histogram_.size();
  size_t program_size = pattern->program_histogram_.size();
  base::StringAppendF(&s, "  model shingles %" PRIuS "\n", model_size);
  s += HistogramToStringFull(pattern->model_histogram_, "    ", max);
  base::StringAppendF(&s, "  program shingles %" PRIuS "\n", program_size);
  s += HistogramToStringFull(pattern->program_histogram_, "    ", max);
  return s;
}

struct ShinglePatternIndexLess {
  bool operator()(const ShinglePattern::Index& a,
                  const ShinglePattern::Index& b) const {
    if (a.hash_ < b.hash_) return true;
    if (a.hash_ > b.hash_) return false;

    for (uint8_t i = 0; i < Shingle::kWidth; ++i) {
      if (a.kinds_[i] < b.kinds_[i]) return true;
      if (a.kinds_[i] > b.kinds_[i]) return false;
      if ((a.kinds_[i] & ShinglePattern::kVariable) == 0) {
        if (a.assigned_indexes_[i] < b.assigned_indexes_[i])
          return true;
        if (a.assigned_indexes_[i] > b.assigned_indexes_[i])
          return false;
      }
    }
    return false;
  }
};

static uint32_t hash_combine(uint32_t h, uint32_t v) {
  h += v;
  return (h * (37 + 0x0000d100)) ^ (h >> 13);
}

ShinglePattern::Index::Index(const Shingle* instance) {
  uint32_t hash = 0;
  variables_ = 0;
  unique_variables_ = 0;
  first_variable_index_ = 255;

  for (uint8_t i = 0; i < Shingle::kWidth; ++i) {
    LabelInfo* info = instance->at(i);
    uint8_t kind = 0;
    int code = -1;
    uint8_t j = 0;
    for ( ; j < i; ++j) {
      if (info == instance->at(j)) {  // Duplicate LabelInfo
        kind = kinds_[j];
        break;
      }
    }
    if (j == i) {  // Not found above.
      if (info->assignment_) {
        code = info->label_->index_;
        assigned_indexes_[i] = code;
        kind = kFixed + i;
      } else {
        kind = kVariable + i;
        ++unique_variables_;
        if (i < first_variable_index_)
          first_variable_index_ = i;
      }
    }
    if (kind & kVariable) ++variables_;
    hash = hash_combine(hash, code);
    hash = hash_combine(hash, kind);
    kinds_[i] = kind;
    assigned_indexes_[i] = code;
  }
  hash_ = hash;
}

struct ShinglePatternLess {
  bool operator()(const ShinglePattern& a, const ShinglePattern& b) const {
    return index_less(*a.index_, *b.index_);
  }
  ShinglePatternIndexLess index_less;
};

struct ShinglePatternPointerLess {
  bool operator()(const ShinglePattern* a, const ShinglePattern* b) const {
    return pattern_less(*a, *b);
  }
  ShinglePatternLess pattern_less;
};

template<int (*Scorer)(const ShinglePattern*)>
struct OrderShinglePatternByScoreDescending {
  bool operator()(const ShinglePattern* a, const ShinglePattern* b) const {
    int score_a = Scorer(a);
    int score_b = Scorer(b);
    if (score_a > score_b) return true;
    if (score_a < score_b) return false;
    return break_ties(a, b);
  }
  ShinglePatternPointerLess break_ties;
};

// Returns a score for a 'Single Use' rule.  Returns -1 if the rule is not
// applicable.
int SingleUseScore(const ShinglePattern* pattern) {
  if (pattern->index_->variables_ != 1)
    return -1;

  if (pattern->model_histogram_.size() != 1 ||
      pattern->program_histogram_.size() != 1)
    return -1;

  // Does this pattern account for all uses of the variable?
  const ShinglePattern::FreqView& program_freq =
      *pattern->program_histogram_.begin();
  const ShinglePattern::FreqView& model_freq =
      *pattern->model_histogram_.begin();
  int p1 = program_freq.count();
  int m1 = model_freq.count();
  if (p1 == m1) {
    const Shingle* program_instance = program_freq.instance();
    const Shingle* model_instance = model_freq.instance();
    size_t variable_index = pattern->index_->first_variable_index_;
    LabelInfo* program_info = program_instance->at(variable_index);
    LabelInfo* model_info = model_instance->at(variable_index);
    if (!program_info->assignment_) {
      if (program_info->refs_ == p1 && model_info->refs_ == m1) {
        return p1;
      }
    }
  }
  return -1;
}

// The VariableQueue is a priority queue of unassigned LabelInfos from
// the 'program' (the 'variables') and their AssignmentCandidates.
class VariableQueue {
 public:
  typedef std::pair<int, LabelInfo*> ScoreAndLabel;

  VariableQueue() = default;

  bool empty() const { return queue_.empty(); }

  const ScoreAndLabel& first() const { return *queue_.begin(); }

  // For debugging only.
  void Print() const {
    for (Queue::const_iterator p = queue_.begin();  p != queue_.end();  ++p) {
      AssignmentCandidates* candidates = p->second->candidates();
      candidates->Print(std::numeric_limits<int>::max());
    }
  }

  void AddPendingUpdate(LabelInfo* program_info, LabelInfo* model_info,
                        int delta_score) {
    AssignmentCandidates* candidates = program_info->candidates();
    if (!candidates->HasPendingUpdates()) {
      pending_update_candidates_.push_back(candidates);
    }
    candidates->AddPendingUpdate(model_info, delta_score);
  }

  void ApplyPendingUpdates() {
    for (size_t i = 0;  i < pending_update_candidates_.size();  ++i) {
      AssignmentCandidates* candidates = pending_update_candidates_[i];
      int old_score = candidates->TopScore();
      queue_.erase(ScoreAndLabel(old_score, candidates->program_info()));
      candidates->ApplyPendingUpdates();
      if (!candidates->empty()) {
        int new_score = candidates->TopScore();
        queue_.insert(ScoreAndLabel(new_score, candidates->program_info()));
      }
    }
    pending_update_candidates_.clear();
  }

 private:
  struct OrderScoreAndLabelByScoreDecreasing {
    bool operator()(const ScoreAndLabel& a, const ScoreAndLabel& b) const {
      if (a.first > b.first) return true;
      if (a.first < b.first) return false;
      return OrderLabelInfo()(a.second, b.second);
    }
  };
  typedef std::set<ScoreAndLabel, OrderScoreAndLabelByScoreDecreasing> Queue;

  Queue queue_;
  std::vector<AssignmentCandidates*> pending_update_candidates_;

  DISALLOW_COPY_AND_ASSIGN(VariableQueue);
};


class AssignmentProblem {
 public:
  AssignmentProblem(const Trace& trace, size_t model_end)
      : trace_(trace),
        model_end_(model_end) {
    VLOG(2) << "AssignmentProblem::AssignmentProblem  " << model_end << ", "
            << trace.size();
  }

  bool Solve() {
    if (model_end_ < Shingle::kWidth ||
        trace_.size() - model_end_ < Shingle::kWidth) {
      // Nothing much we can do with such a short problem.
      return true;
    }
    instances_.resize(trace_.size() - Shingle::kWidth + 1, nullptr);
    AddShingles(0, model_end_);
    AddShingles(model_end_, trace_.size());
    InitialClassify();
    AddPatternsNeedingUpdatesToQueues();

    patterns_needing_updates_.clear();
    while (FindAndAssignBestLeader())
      patterns_needing_updates_.clear();
    PrintActivePatterns();

    return true;
  }

 private:
  typedef std::set<Shingle*, Shingle::PointerLess> ShingleSet;

  typedef std::set<const ShinglePattern*, ShinglePatternPointerLess>
      ShinglePatternSet;

  // Patterns are partitioned into the following sets:

  // * Retired patterns (not stored).  No shingles exist for this pattern (they
  //   all now match more specialized patterns).
  // * Useless patterns (not stored).  There are no 'program' shingles for this
  //   pattern (they all now match more specialized patterns).
  // * Single-use patterns - single_use_pattern_queue_.
  // * Other patterns - active_non_single_use_patterns_ / variable_queue_.

  typedef std::set<const ShinglePattern*,
                   OrderShinglePatternByScoreDescending<&SingleUseScore> >
      SingleUsePatternQueue;

  void PrintPatternsHeader() const {
    VLOG(2) << shingle_instances_.size() << " instances  "
            << trace_.size() << " trace length  "
            << patterns_.size() << " shingle indexes  "
            << single_use_pattern_queue_.size() << " single use patterns  "
            << active_non_single_use_patterns_.size() << " active patterns";
  }

  void PrintActivePatterns() const {
    for (ShinglePatternSet::const_iterator p =
             active_non_single_use_patterns_.begin();
         p != active_non_single_use_patterns_.end();
         ++p) {
      const ShinglePattern* pattern = *p;
      VLOG(2) << ToString(pattern, 10);
    }
  }

  void PrintPatterns() const {
    PrintAllPatterns();
    PrintActivePatterns();
    PrintAllShingles();
  }

  void PrintAllPatterns() const {
    for (IndexToPattern::const_iterator p = patterns_.begin();
         p != patterns_.end();
         ++p) {
      const ShinglePattern& pattern = p->second;
      VLOG(2) << ToString(&pattern, 10);
    }
  }

  void PrintAllShingles() const {
    for (Shingle::OwningSet::const_iterator p = shingle_instances_.begin();
         p != shingle_instances_.end();
         ++p) {
      const Shingle& instance = *p;
      VLOG(2) << ToString(&instance) << "   " << ToString(instance.pattern());
    }
  }


  void AddShingles(size_t begin, size_t end) {
    for (size_t i = begin; i + Shingle::kWidth - 1 < end; ++i) {
      instances_[i] = Shingle::Find(trace_, i, &shingle_instances_);
    }
  }

  void Declassify(Shingle* shingle) {
    ShinglePattern* pattern = shingle->pattern();
    if (shingle->InModel()) {
      pattern->model_histogram_.erase(ShinglePattern::FreqView(shingle));
      pattern->model_coverage_ -= shingle->position_count();
    } else {
      pattern->program_histogram_.erase(ShinglePattern::FreqView(shingle));
      pattern->program_coverage_ -= shingle->position_count();
    }
    shingle->set_pattern(nullptr);
  }

  void Reclassify(Shingle* shingle) {
    ShinglePattern* pattern = shingle->pattern();
    LOG_ASSERT(pattern == nullptr);

    ShinglePattern::Index index(shingle);
    if (index.variables_ == 0)
      return;

    std::pair<IndexToPattern::iterator, bool> inserted =
        patterns_.insert(std::make_pair(index, ShinglePattern()));

    pattern = &inserted.first->second;
    pattern->index_ = &inserted.first->first;
    shingle->set_pattern(pattern);
    patterns_needing_updates_.insert(pattern);

    if (shingle->InModel()) {
      pattern->model_histogram_.insert(ShinglePattern::FreqView(shingle));
      pattern->model_coverage_ += shingle->position_count();
    } else {
      pattern->program_histogram_.insert(ShinglePattern::FreqView(shingle));
      pattern->program_coverage_ += shingle->position_count();
    }
  }

  void InitialClassify() {
    for (Shingle::OwningSet::iterator p = shingle_instances_.begin();
         p != shingle_instances_.end();
         ++p) {
      // GCC's set<T>::iterator::operator *() returns a const object.
      Reclassify(const_cast<Shingle*>(&*p));
    }
  }

  // For the positions in |info|, find the shingles that overlap that position.
  void AddAffectedPositions(LabelInfo* info, ShingleSet* affected_shingles) {
    const uint8_t kWidth = Shingle::kWidth;
    for (size_t i = 0;  i < info->positions_.size();  ++i) {
      size_t position = info->positions_[i];
      // Find bounds to the subrange of |trace_| we are in.
      size_t start = position < model_end_ ? 0 : model_end_;
      size_t end = position < model_end_ ? model_end_ : trace_.size();

      // Clip [position-kWidth+1, position+1)
      size_t low =
          position > start + kWidth - 1 ? position - kWidth + 1 : start;
      size_t high = position + kWidth < end ? position + 1 : end - kWidth + 1;

      for (size_t shingle_position = low;
           shingle_position < high;
           ++shingle_position) {
        Shingle* overlapping_shingle = instances_.at(shingle_position);
        affected_shingles->insert(overlapping_shingle);
      }
    }
  }

  void RemovePatternsNeedingUpdatesFromQueues() {
    for (ShinglePatternSet::iterator p = patterns_needing_updates_.begin();
         p != patterns_needing_updates_.end();
         ++p) {
      RemovePatternFromQueues(*p);
    }
  }

  void AddPatternsNeedingUpdatesToQueues() {
    for (ShinglePatternSet::iterator p = patterns_needing_updates_.begin();
         p != patterns_needing_updates_.end();
         ++p) {
      AddPatternToQueues(*p);
    }
    variable_queue_.ApplyPendingUpdates();
  }

  void RemovePatternFromQueues(const ShinglePattern* pattern) {
    int single_use_score = SingleUseScore(pattern);
    if (single_use_score > 0) {
      size_t n = single_use_pattern_queue_.erase(pattern);
      LOG_ASSERT(n == 1);
    } else if (pattern->program_histogram_.empty() &&
               pattern->model_histogram_.empty()) {
      NOTREACHED();  // Should not come back to life.
    } else if (pattern->program_histogram_.empty()) {
      // Useless pattern.
    } else {
      active_non_single_use_patterns_.erase(pattern);
      AddPatternToLabelQueue(pattern, -1);
    }
  }

  void AddPatternToQueues(const ShinglePattern* pattern) {
    int single_use_score = SingleUseScore(pattern);
    if (single_use_score > 0) {
      single_use_pattern_queue_.insert(pattern);
    } else if (pattern->program_histogram_.empty() &&
               pattern->model_histogram_.empty()) {
    } else if (pattern->program_histogram_.empty()) {
      // Useless pattern.
    } else {
      active_non_single_use_patterns_.insert(pattern);
      AddPatternToLabelQueue(pattern, +1);
    }
  }

  void AddPatternToLabelQueue(const ShinglePattern* pattern, int sign) {
    // For each possible assignment in this pattern, update the potential
    // contributions to the LabelInfo queues.

    // We want to find for each symbol (LabelInfo) the maximum contribution that
    // could be achieved by making shingle-wise assignments between shingles in
    // the model and shingles in the program.
    //
    // If the shingles in the histograms are independent (no two shingles have a
    // symbol in common) then any permutation of the assignments is possible,
    // and the maximum contribution can be found by taking the maximum over all
    // the pairs.
    //
    // If the shingles are dependent two things happen.  The maximum
    // contribution to any given symbol is a sum because the symbol has
    // contributions from all the shingles containing it.  Second, some
    // assignments are blocked by previous incompatible assignments.  We want to
    // avoid a combinatorial search, so we ignore the blocking.

    const size_t kUnwieldy = 5;

    typedef std::map<LabelInfo*, int> LabelToScore;
    typedef std::map<LabelInfo*, LabelToScore > ScoreSet;
    ScoreSet maxima;

    size_t n_model_samples = 0;
    for (ShinglePattern::Histogram::const_iterator model_iter =
             pattern->model_histogram_.begin();
         model_iter != pattern->model_histogram_.end();
         ++model_iter) {
      if (++n_model_samples > kUnwieldy) break;
      const ShinglePattern::FreqView& model_freq = *model_iter;
      int m1 = model_freq.count();
      const Shingle* model_instance = model_freq.instance();

      ScoreSet sums;
      size_t n_program_samples = 0;
      for (ShinglePattern::Histogram::const_iterator program_iter =
               pattern->program_histogram_.begin();
           program_iter != pattern->program_histogram_.end();
           ++program_iter) {
        if (++n_program_samples > kUnwieldy) break;
        const ShinglePattern::FreqView& program_freq = *program_iter;
        int p1 = program_freq.count();
        const Shingle* program_instance = program_freq.instance();

        // int score = p1;  // ? weigh all equally??
        int score = std::min(p1, m1);

        for (uint8_t i = 0; i < Shingle::kWidth; ++i) {
          LabelInfo* program_info = program_instance->at(i);
          LabelInfo* model_info = model_instance->at(i);
          if ((model_info->assignment_ == nullptr) !=
              (program_info->assignment_ == nullptr)) {
            VLOG(2) << "ERROR " << i
                    << "\n\t"  << ToString(pattern, 10)
                    << "\n\t" << ToString(program_instance)
                    << "\n\t" << ToString(model_instance);
          }
          if (!program_info->assignment_ && !model_info->assignment_) {
            sums[program_info][model_info] += score;
          }
        }
      }

      for (ScoreSet::iterator assignee_iterator = sums.begin();
           assignee_iterator != sums.end();
           ++assignee_iterator) {
        LabelInfo* program_info = assignee_iterator->first;
        for (LabelToScore::iterator p = assignee_iterator->second.begin();
             p != assignee_iterator->second.end();
             ++p) {
          LabelInfo* model_info = p->first;
          int score = p->second;
          int* slot = &maxima[program_info][model_info];
          *slot = std::max(*slot, score);
        }
      }
    }

    for (ScoreSet::iterator assignee_iterator = maxima.begin();
         assignee_iterator != maxima.end();
         ++assignee_iterator) {
      LabelInfo* program_info = assignee_iterator->first;
      for (LabelToScore::iterator p = assignee_iterator->second.begin();
           p != assignee_iterator->second.end();
           ++p) {
        LabelInfo* model_info = p->first;
        int score = sign * p->second;
        variable_queue_.AddPendingUpdate(program_info, model_info, score);
      }
    }
  }

  void AssignOne(LabelInfo* model_info, LabelInfo* program_info) {
    LOG_ASSERT(!model_info->assignment_);
    LOG_ASSERT(!program_info->assignment_);
    LOG_ASSERT(model_info->is_model_);
    LOG_ASSERT(!program_info->is_model_);

    VLOG(3) << "Assign " << ToString(program_info)
            << " := " << ToString(model_info);

    ShingleSet affected_shingles;
    AddAffectedPositions(model_info, &affected_shingles);
    AddAffectedPositions(program_info, &affected_shingles);

    for (ShingleSet::iterator p = affected_shingles.begin();
         p != affected_shingles.end();
         ++p) {
      patterns_needing_updates_.insert((*p)->pattern());
    }

    RemovePatternsNeedingUpdatesFromQueues();

    for (ShingleSet::iterator p = affected_shingles.begin();
         p != affected_shingles.end();
         ++p) {
      Declassify(*p);
    }

    program_info->label_->index_ = model_info->label_->index_;
    // Mark as assigned
    model_info->assignment_ = program_info;
    program_info->assignment_ = model_info;

    for (ShingleSet::iterator p = affected_shingles.begin();
         p != affected_shingles.end();
         ++p) {
      Reclassify(*p);
    }

    AddPatternsNeedingUpdatesToQueues();
  }

  bool AssignFirstVariableOfHistogramHead(const ShinglePattern& pattern) {
    const ShinglePattern::FreqView& program_1 =
        *pattern.program_histogram_.begin();
    const ShinglePattern::FreqView& model_1 = *pattern.model_histogram_.begin();
    const Shingle* program_instance = program_1.instance();
    const Shingle* model_instance = model_1.instance();
    size_t variable_index = pattern.index_->first_variable_index_;
    LabelInfo* program_info = program_instance->at(variable_index);
    LabelInfo* model_info = model_instance->at(variable_index);
    AssignOne(model_info, program_info);
    return true;
  }

  bool FindAndAssignBestLeader() {
    LOG_ASSERT(patterns_needing_updates_.empty());

    if (!single_use_pattern_queue_.empty()) {
      const ShinglePattern& pattern = **single_use_pattern_queue_.begin();
      return AssignFirstVariableOfHistogramHead(pattern);
    }

    if (variable_queue_.empty())
      return false;

    const VariableQueue::ScoreAndLabel best = variable_queue_.first();
    int score = best.first;
    LabelInfo* assignee = best.second;

    // TODO(sra): score (best.first) can be zero.  A zero score means we are
    // blindly picking between two (or more) alternatives which look the same.
    // If we exit on the first zero-score we sometimes get 3-4% better total
    // compression.  This indicates that 'infill' is doing a better job than
    // picking blindly.  Perhaps we can use an extended region around the
    // undistinguished competing alternatives to break the tie.
    if (score == 0) {
      variable_queue_.Print();
      return false;
    }

    AssignmentCandidates* candidates = assignee->candidates();
    if (candidates->empty())
      return false;  // Should not happen.

    AssignOne(candidates->top_candidate(), assignee);
    return true;
  }

 private:
  // The trace vector contains the model sequence [0, model_end_) followed by
  // the program sequence [model_end_, trace.end())
  const Trace& trace_;
  size_t model_end_;

  // |shingle_instances_| is the set of 'interned' shingles.
  Shingle::OwningSet shingle_instances_;

  // |instances_| maps from position in |trace_| to Shingle at that position.
  std::vector<Shingle*> instances_;

  SingleUsePatternQueue single_use_pattern_queue_;
  ShinglePatternSet active_non_single_use_patterns_;
  VariableQueue variable_queue_;

  // Transient information: when we make an assignment, we need to recompute
  // priority queue information derived from these ShinglePatterns.
  ShinglePatternSet patterns_needing_updates_;

  typedef std::map<ShinglePattern::Index,
                   ShinglePattern, ShinglePatternIndexLess> IndexToPattern;
  IndexToPattern patterns_;

  DISALLOW_COPY_AND_ASSIGN(AssignmentProblem);
};

class Adjuster : public AdjustmentMethod {
 public:
  Adjuster() : prog_(nullptr), model_(nullptr) {}
  ~Adjuster() = default;

  bool Adjust(const AssemblyProgram& model, AssemblyProgram* program) {
    VLOG(1) << "Adjuster::Adjust";
    prog_ = program;
    model_ = &model;
    return Finish();
  }

  bool Finish() {
    prog_->UnassignIndexes();
    Trace abs32_trace_;
    Trace rel32_trace_;
    CollectTraces(model_, &abs32_trace_, &rel32_trace_, true);
    size_t abs32_model_end = abs32_trace_.size();
    size_t rel32_model_end = rel32_trace_.size();
    CollectTraces(prog_,  &abs32_trace_,  &rel32_trace_,  false);
    Solve(abs32_trace_, abs32_model_end);
    Solve(rel32_trace_, rel32_model_end);
    prog_->AssignRemainingIndexes();
    return true;
  }

 private:
  void CollectTraces(const AssemblyProgram* program, Trace* abs32, Trace* rel32,
                     bool is_model) {
    label_info_maker_.ResetDebugLabel();

    for (Label* label : program->abs32_label_annotations())
      ReferenceLabel(abs32, is_model, label);
    for (Label* label : program->rel32_label_annotations())
      ReferenceLabel(rel32, is_model, label);

    // TODO(sra): we could simply append all the labels in index order to
    // incorporate some costing for entropy (bigger deltas) that will be
    // introduced into the label address table by non-monotonic ordering.  This
    // would have some knock-on effects to parts of the algorithm that work on
    // single-occurrence labels.
  }

  void Solve(const Trace& model, size_t model_end) {
    base::Time start_time = base::Time::Now();
    AssignmentProblem a(model, model_end);
    a.Solve();
    VLOG(1) << " Adjuster::Solve "
            << (base::Time::Now() - start_time).InSecondsF();
  }

  void ReferenceLabel(Trace* trace, bool is_model, Label* label) {
    trace->push_back(label_info_maker_.MakeLabelInfo(
        label, is_model, static_cast<uint32_t>(trace->size())));
  }

  AssemblyProgram* prog_;         // Program to be adjusted, owned by caller.
  const AssemblyProgram* model_;  // Program to be mimicked, owned by caller.

  LabelInfoMaker label_info_maker_;

 private:
  DISALLOW_COPY_AND_ASSIGN(Adjuster);
};

////////////////////////////////////////////////////////////////////////////////

}  // namespace adjustment_method_2

AdjustmentMethod* AdjustmentMethod::MakeShingleAdjustmentMethod() {
  return new adjustment_method_2::Adjuster();
}

}  // namespace courgette
