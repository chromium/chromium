// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_ATTRIBUTION_REPORTING_ATTRIBUTION_METRICS_H_
#define CONTENT_BROWSER_ATTRIBUTION_REPORTING_ATTRIBUTION_METRICS_H_

namespace content {

void RecordRegisterConversionAllowed(bool allowed);

void RecordRegisterImpressionAllowed(bool allowed);

}  // namespace content

#endif  // CONTENT_BROWSER_ATTRIBUTION_REPORTING_ATTRIBUTION_METRICS_H_
