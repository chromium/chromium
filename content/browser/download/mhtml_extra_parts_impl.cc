// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/download/mhtml_extra_parts_impl.h"

namespace {
// Only the address of this variable is used.  It is used as a key to UserData.
const int kMHTMLExtraPartsKey = 0;
}

namespace content {

MHTMLExtraDataPart::MHTMLExtraDataPart() = default;
MHTMLExtraDataPart::MHTMLExtraDataPart(const MHTMLExtraDataPart& other) =
    default;
MHTMLExtraDataPart::MHTMLExtraDataPart(MHTMLExtraDataPart&& other) = default;
MHTMLExtraDataPart& MHTMLExtraDataPart::operator=(
    const MHTMLExtraDataPart& other) = default;
MHTMLExtraDataPart& MHTMLExtraDataPart::operator=(MHTMLExtraDataPart&& other) =
    default;
MHTMLExtraDataPart::~MHTMLExtraDataPart() = default;

MHTMLExtraPartsImpl::MHTMLExtraPartsImpl() = default;

MHTMLExtraPartsImpl::~MHTMLExtraPartsImpl() = default;

MHTMLExtraParts* MHTMLExtraParts::FromWebContents(WebContents* contents) {
  // Get the MHTMLExtraPartsImpl from the web contents.
  MHTMLExtraPartsImpl* extra_data_impl = static_cast<MHTMLExtraPartsImpl*>(
      contents->GetUserData(&kMHTMLExtraPartsKey));

  // If we did not have one on the web contents already, make one and put it on
  // the web contents.
  if (extra_data_impl == nullptr) {
    extra_data_impl = new MHTMLExtraPartsImpl();
    contents->SetUserData(&kMHTMLExtraPartsKey,
                          std::unique_ptr<MHTMLExtraParts>(
                              static_cast<MHTMLExtraParts*>(extra_data_impl)));
  }
  return static_cast<MHTMLExtraParts*>(extra_data_impl);
}

int64_t MHTMLExtraPartsImpl::size() {
  return parts_.size();
}

void MHTMLExtraPartsImpl::AddExtraMHTMLPart(const std::string& content_type,
                                            const std::string& content_location,
                                            const std::string& extra_headers,
                                            const std::string& body) {
  MHTMLExtraDataPart part;
  part.content_type = content_type;
  part.content_location = content_location;
  part.extra_headers = extra_headers;
  part.body = body;

  // Add this part to the list of parts to be saved out when the file is
  // written.
  parts_.emplace_back(std::move(part));
}

}  // namespace content
