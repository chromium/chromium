// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_PUBLIC_BROWSER_MHTML_EXTRA_PARTS_H_
#define CONTENT_PUBLIC_BROWSER_MHTML_EXTRA_PARTS_H_

#include "base/supports_user_data.h"
#include "content/common/content_export.h"
#include "content/public/browser/web_contents.h"

namespace content {

// Interface class for adding an extra part to MHTML when doing MHTML
// generation.  To use it, get it from the web contents with FromWebContents,
// then call AddExtraMHTMLPart to add a new part.
class CONTENT_EXPORT MHTMLExtraParts : public base::SupportsUserData::Data {
 public:
  // Retrieves the extra data from the web contents.
  static MHTMLExtraParts* FromWebContents(WebContents* contents);

  // Add an extra MHTML part to the data structure stored by the WebContents.
  // This will take care of generating the boundary line.  This will also set
  // the content-type and content-location headers to the values provided, and
  // use the body provided.
  virtual void AddExtraMHTMLPart(const std::string& content_type,
                                 const std::string& content_location,
                                 const std::string& extra_headers,
                                 const std::string& body) = 0;

  // Returns the number of extra parts added.
  virtual int64_t size() = 0;
};

}  // namespace content

#endif  // CONTENT_PUBLIC_BROWSER_MHTML_EXTRA_PARTS_H_
