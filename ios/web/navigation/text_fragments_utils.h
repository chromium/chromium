// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_WEB_NAVIGATION_TEXT_FRAGMENTS_UTILS_H_
#define IOS_WEB_NAVIGATION_TEXT_FRAGMENTS_UTILS_H_

#include "base/values.h"

class GURL;

namespace web {

// This file contains helper functions relating to Text Fragments, which are
// appended to the reference fragment in the URL and instruct the user agent
// to highlight a given snippet of text and the page and scroll it into view.
// See also: https://wicg.github.io/scroll-to-text-fragment/

// Checks the fragment portion of the URL for Text Fragments. Returns zero or
// more dictionaries containing the parsed parameters used by the fragment-
// finding algorithm, as defined in the spec.
base::Value ParseTextFragments(const GURL& url);

// Extracts the text fragments, if any, from a ref string.
std::vector<std::string> ExtractTextFragments(std::string ref_string);

// Breaks a text fragment into its component parts, as needed for the algorithm
// described in the spec. Returns a dictionary Value, or a None Value if the
// fragment is malformed.
base::Value TextFragmentToValue(std::string fragment);

}  // namespace web

#endif  // IOS_WEB_NAVIGATION_TEXT_FRAGMENTS_UTILS_H_
