// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_DOWNLOAD_MHTML_EXTRA_PARTS_IMPL_H_
#define CONTENT_BROWSER_DOWNLOAD_MHTML_EXTRA_PARTS_IMPL_H_

#include "content/public/browser/mhtml_extra_parts.h"

namespace content {

// Data fields used to build an additional MHTML part in the output file.
struct MHTMLExtraDataPart {
  std::string content_type;
  std::string content_location;
  std::string extra_headers;
  std::string body;

  MHTMLExtraDataPart();
  MHTMLExtraDataPart(const MHTMLExtraDataPart& other);
  MHTMLExtraDataPart(MHTMLExtraDataPart&& other);
  MHTMLExtraDataPart& operator=(const MHTMLExtraDataPart& other);
  MHTMLExtraDataPart& operator=(MHTMLExtraDataPart&& other);
  ~MHTMLExtraDataPart();
};

// Class used as a data object for WebContents UserData to represent an MHTML
// part that we plan to write into the output MHTML file.  Each MHTMLExtraPart
// object in the contained vector lets us hold enough information to generate
// one MHTML part.  This allows arbitrary extra MHTML parts to be added into the
// complete file.  For instance, this might be used for gathering load time
// signals in debug code for analysis.
class MHTMLExtraPartsImpl : public content::MHTMLExtraParts {
 public:
  MHTMLExtraPartsImpl();
  ~MHTMLExtraPartsImpl() override;

  // Return the vector of parts to be serialized.
  const std::vector<MHTMLExtraDataPart>& parts() const { return parts_; }

  // Return the number of extra parts added.
  int64_t size() override;

  // Creates a MHTMLExtraDataPart and adds it to our vector of parts.
  void AddExtraMHTMLPart(const std::string& content_type,
                         const std::string& content_location,
                         const std::string& extra_headers,
                         const std::string& body) override;

 private:
  std::vector<MHTMLExtraDataPart> parts_;
};

}  // namespace content

#endif  // CONTENT_BROWSER_DOWNLOAD_MHTML_EXTRA_PARTS_IMPL_H_
