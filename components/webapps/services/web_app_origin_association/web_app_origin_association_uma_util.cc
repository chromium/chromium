// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/webapps/services/web_app_origin_association/web_app_origin_association_uma_util.h"

#include "base/metrics/histogram_functions.h"

namespace webapps {

void WebAppOriginAssociationMetrics::RecordFetchResult(FetchResult result) {
  base::UmaHistogramEnumeration("Webapp.WebAppOriginAssociationFetchResult",
                                result);
}

void WebAppOriginAssociationMetrics::RecordParseResult(ParseResult result) {
  base::UmaHistogramEnumeration("Webapp.WebAppOriginAssociationParseResult",
                                result);
}

}  // namespace webapps
