// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_EMBEDDER_ISOLATION_INFO_H_
#define CONTENT_BROWSER_EMBEDDER_ISOLATION_INFO_H_

#include <compare>
#include <cstdint>
#include <optional>
#include <string>

#include "content/common/content_export.h"

namespace content {

// Captures embedder-specified process isolation policy that does not belong
// to the web platform (sandbox, COOP/COEP, OAC) but is imposed by the
// browser/content embedder for security reasons.
//
// Modes are mutually exclusive by construction:
//   - kNone:           no embedder-imposed isolation (the common case).
//   - kPdf:            built-in PDF viewer. Grouped by site/origin so
//                      multiple PDFs of the same site can share a process.
//                      JIT is disabled.
//   - kUniqueInstance: per-document isolation. Each navigation gets a fresh
//                      instance id, producing a unique SiteInfo so that no
//                      two viewers share a process. Currently used for MIME
//                      handler extension viewers.
//   - kPrivileged:     a blessed feature (see //chrome's
//                      PrivilegedWebContents) whose content is granted
//                      elevated browser capabilities. Grouped by
//                      `feature_id` (a constant per feature, not a
//                      per-navigation id) so that instances of the same
//                      privileged feature can share a process, while never
//                      sharing a process with ordinary (kNone) content of
//                      the same site. Forces a dedicated process; JIT is
//                      left ON (the content is trusted and wants
//                      performance, unlike kPdf).
class CONTENT_EXPORT EmbedderIsolationInfo {
 public:
  enum class Mode {
    kNone,
    kPdf,
    kUniqueInstance,
    kPrivileged,
  };

  static EmbedderIsolationInfo CreateNone();

  // kPdf factory. PDFs are grouped by site, not per-instance.
  static EmbedderIsolationInfo CreateForPdf();

  // kUniqueInstance factory. `instance_id` must be non-negative and unique
  // per "instance" the embedder wants to isolate; the caller is responsible
  // for sourcing it.
  static EmbedderIsolationInfo CreateForUniqueInstance(int64_t instance_id);

  // kPrivileged factory. `feature_id` identifies the blessed feature and is
  // constant across all instances of that feature (unlike kUniqueInstance's
  // per-navigation id), so instances with the same `feature_id` compare
  // equal and may share a process. Must be non-negative.
  static EmbedderIsolationInfo CreateForPrivileged(int64_t feature_id);

  EmbedderIsolationInfo(const EmbedderIsolationInfo&);
  EmbedderIsolationInfo& operator=(const EmbedderIsolationInfo&);
  ~EmbedderIsolationInfo();

  Mode mode() const { return mode_; }
  bool is_none() const { return mode_ == Mode::kNone; }
  bool is_pdf() const { return mode_ == Mode::kPdf; }
  bool is_unique_instance() const { return mode_ == Mode::kUniqueInstance; }
  bool is_privileged() const { return mode_ == Mode::kPrivileged; }

  // Returns the instance id when mode() == kUniqueInstance, otherwise
  // `std::nullopt`.
  std::optional<int64_t> instance_id() const {
    return mode_ == Mode::kUniqueInstance ? std::optional<int64_t>(instance_id_)
                                          : std::nullopt;
  }

  // Returns the feature id when mode() == kPrivileged, otherwise
  // `std::nullopt`.
  std::optional<int64_t> privileged_feature_id() const {
    return mode_ == Mode::kPrivileged ? std::optional<int64_t>(instance_id_)
                                      : std::nullopt;
  }

  std::string ToDebugString() const;

  friend bool operator==(const EmbedderIsolationInfo&,
                         const EmbedderIsolationInfo&) = default;
  friend std::weak_ordering operator<=>(const EmbedderIsolationInfo&,
                                        const EmbedderIsolationInfo&) = default;

 private:
  EmbedderIsolationInfo();
  EmbedderIsolationInfo(Mode mode, int64_t instance_id);

  Mode mode_ = Mode::kNone;
  int64_t instance_id_ = 0;
};

}  // namespace content

#endif  // CONTENT_BROWSER_EMBEDDER_ISOLATION_INFO_H_
