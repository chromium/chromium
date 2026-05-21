// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_WEBID_DELEGATION_EVP_METRICS_H_
#define CONTENT_BROWSER_WEBID_DELEGATION_EVP_METRICS_H_

#include "content/common/content_export.h"

namespace content::webid {

enum class EvpRequestStatus;

CONTENT_EXPORT void RecordEvpRequestStatus(EvpRequestStatus status);

}  // namespace content::webid

#endif  // CONTENT_BROWSER_WEBID_DELEGATION_EVP_METRICS_H_
