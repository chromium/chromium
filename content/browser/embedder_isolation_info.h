// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_EMBEDDER_ISOLATION_INFO_H_
#define CONTENT_BROWSER_EMBEDDER_ISOLATION_INFO_H_

#include <compare>
#include <cstdint>
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
class CONTENT_EXPORT EmbedderIsolationInfo {
 public:
  enum class Mode {
    kNone,
    kPdf,
    kUniqueInstance,
  };

  // kNone factory (equivalent to default constructor).
  static EmbedderIsolationInfo CreateNone();

  // kPdf factory. PDFs are grouped by site, not per-instance.
  static EmbedderIsolationInfo CreateForPdf();

  // kUniqueInstance factory. `instance_id` must be non-negative and unique
  // per navigation; in practice this is NavigationRequest::navigation_id_,
  // which is monotonic and never negative.
  static EmbedderIsolationInfo CreateForUniqueInstance(int64_t instance_id);

  EmbedderIsolationInfo();
  EmbedderIsolationInfo(const EmbedderIsolationInfo&);
  EmbedderIsolationInfo& operator=(const EmbedderIsolationInfo&);
  ~EmbedderIsolationInfo();

  Mode mode() const { return mode_; }
  bool is_none() const { return mode_ == Mode::kNone; }
  bool is_pdf() const { return mode_ == Mode::kPdf; }
  bool is_unique_instance() const { return mode_ == Mode::kUniqueInstance; }

  // Only valid when mode() == kUniqueInstance. CHECKs otherwise.
  int64_t instance_id() const;

  std::string ToDebugString() const;

  friend bool operator==(const EmbedderIsolationInfo&,
                         const EmbedderIsolationInfo&) = default;
  friend std::weak_ordering operator<=>(const EmbedderIsolationInfo&,
                                        const EmbedderIsolationInfo&) = default;

 private:
  EmbedderIsolationInfo(Mode mode, int64_t instance_id);

  Mode mode_ = Mode::kNone;
  int64_t instance_id_ = 0;
};

}  // namespace content

#endif  // CONTENT_BROWSER_EMBEDDER_ISOLATION_INFO_H_
