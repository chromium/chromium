// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef EXTENSIONS_BROWSER_API_DECLARATIVE_NET_REQUEST_FILTER_LIST_CONVERTER_CONVERTER_H_
#define EXTENSIONS_BROWSER_API_DECLARATIVE_NET_REQUEST_FILTER_LIST_CONVERTER_CONVERTER_H_

#include <vector>

#include "base/files/file_path.h"
#include "base/macros.h"

namespace extensions {
namespace declarative_net_request {
namespace filter_list_converter {

enum WriteType {
  kJSONRuleset,
  kExtension,
};

// Utility function to convert filter list files in the text format to a JSON
// file in a format supported by the Declarative Net Request API. If |type| is
// kExtension, output_path is treated as the extension directory and the ruleset
// is written to "rules.json". Else it is treated as the json ruleset location.
// Returns false if the conversion fails.
bool ConvertRuleset(const std::vector<base::FilePath>& filter_list_inputs,
                    const base::FilePath& output_path,
                    WriteType type,
                    bool noisy = true);

}  // namespace filter_list_converter
}  // namespace declarative_net_request
}  // namespace extensions

#endif  // EXTENSIONS_BROWSER_API_DECLARATIVE_NET_REQUEST_FILTER_LIST_CONVERTER_CONVERTER_H_
