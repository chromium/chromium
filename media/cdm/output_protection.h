// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_CDM_OUTPUT_PROTECTION_H_
#define MEDIA_CDM_OUTPUT_PROTECTION_H_

#include <stdint.h>

#include "base/functional/callback.h"
#include "media/base/media_export.h"

namespace media {

class MEDIA_EXPORT OutputProtection {
 public:
  OutputProtection() = default;

  OutputProtection(const OutputProtection&) = delete;
  OutputProtection& operator=(const OutputProtection&) = delete;

  virtual ~OutputProtection() = default;

  using QueryStatusCB = base::OnceCallback<
      void(bool success, uint32_t link_mask, uint32_t protection_mask)>;
  using EnableProtectionCB = base::OnceCallback<void(bool success)>;

  // Connected output link types returned by QueryStatus(). Must match values
  // in cdm::OutputLinkTypes.
  enum class LinkTypes {
    NONE = 0,
    UNKNOWN = 1,
    INTERNAL = 2,
    VGA = 4,
    HDMI = 8,
    DVI = 16,
    DISPLAYPORT = 32,
    NETWORK = 64,
  };

  // Supported output protection methods for use with EnableProtection() and
  // returned by QueryStatus(). Must match values in
  // cdm::OutputProtectionMethods.
  enum class ProtectionType {
    NONE = 0,
    HDCP = 1,
  };

  // Queries link status and protection status. Clients need to query status
  // periodically in order to detect changes. |callback| will be called with
  // the following values:
  // - success: Whether the query succeeded. If false, values of |link_mask|
  //   and |protection_mask| should be ignored.
  // - link_mask: The type of connected output links, which is a bit-mask of
  //   the LinkType values.
  // - protection_mask: The type of enabled protections, which is a bit-mask
  //   of the ProtectionType values.
  virtual void QueryStatus(QueryStatusCB callback) = 0;

  // Sets desired protection methods. |desired_protection_mask| is a bit-mask
  // of ProtectionType values. Calls |callback| when the request has been made.
  // Users should call QueryStatus() to verify that it was actually applied.
  // Protections will be disabled if no longer desired by all instances.
  // |callback| will be called with the following value:
  // - success: True when the protection request has been made. This may be
  //   before the protection have actually been applied. False if it failed
  //   to make the protection request, and in this case there is no need to
  //   call QueryStatus().
  virtual void EnableProtection(uint32_t desired_protection_mask,
                                EnableProtectionCB callback) = 0;
};

}  // namespace media

#endif  // MEDIA_CDM_OUTPUT_PROTECTION_H_
