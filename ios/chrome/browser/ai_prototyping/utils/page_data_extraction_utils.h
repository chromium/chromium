// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_AI_PROTOTYPING_UTILS_PAGE_DATA_EXTRACTION_UTILS_H_
#define IOS_CHROME_BROWSER_AI_PROTOTYPING_UTILS_PAGE_DATA_EXTRACTION_UTILS_H_

#import "components/optimization_guide/optimization_guide_buildflags.h"

#if BUILDFLAG(BUILD_WITH_INTERNAL_OPTIMIZATION_GUIDE)
#import "components/optimization_guide/proto/features/common_quality_data.pb.h"
#endif

namespace web {
class WebState;
}  // namespace web

namespace page_data_extraction_utils {

#if BUILDFLAG(BUILD_WITH_INTERNAL_OPTIMIZATION_GUIDE)

// Creates and populates the PageContext proto for a given WebState. The caller
// takes ownership of the returned pointer.
optimization_guide::proto::PageContext* GetPageContextForWebState(
    web::WebState* web_state);

#endif  // BUILDFLAG(BUILD_WITH_INTERNAL_OPTIMIZATION_GUIDE)

}  // namespace page_data_extraction_utils

#endif  // IOS_CHROME_BROWSER_AI_PROTOTYPING_UTILS_PAGE_DATA_EXTRACTION_UTILS_H_
