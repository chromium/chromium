// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_TOOLS_CONTENT_DECODER_TOOL_CONTENT_DECODER_TOOL_H_
#define NET_TOOLS_CONTENT_DECODER_TOOL_CONTENT_DECODER_TOOL_H_

#include <ostream>
#include <string>
#include <vector>

namespace net {

// Processes input from |input_stream| and writes result to |output_stream|.
// The input will be decoded with |content_encodings|. If processing is
// successful, return true.
bool ContentDecoderToolProcessInput(std::vector<std::string> content_encodings,
                                    std::istream* input_stream,
                                    std::ostream* output_stream);

}  // namespace net

#endif  // NET_TOOLS_CONTENT_DECODER_TOOL_CONTENT_DECODER_TOOL_H_
