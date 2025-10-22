// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/xml/parser/xhtml_subset.h"

namespace blink {

// Needed for deciding whether extended entities should be enabled for an
// XHTML document. Determines whether the external id matches any of those
// listed in
// https://html.spec.whatwg.org/C/#parsing-xhtml-documents:named-character-references
bool MatchesXHTMLSubsetDTD(String public_id) {
  return public_id == "-//W3C//DTD XHTML 1.0 Transitional//EN" ||
         public_id == "-//W3C//DTD XHTML 1.1//EN" ||
         public_id == "-//W3C//DTD XHTML 1.0 Strict//EN" ||
         public_id == "-//W3C//DTD XHTML 1.0 Frameset//EN" ||
         public_id == "-//W3C//DTD XHTML Basic 1.0//EN" ||
         public_id == "-//W3C//DTD XHTML 1.1 plus MathML 2.0//EN" ||
         public_id ==
             "-//W3C//DTD XHTML 1.1 plus MathML 2.0 plus SVG 1.1//EN" ||
         public_id == "-//W3C//DTD MathML 2.0//EN" ||
         public_id == "-//WAPFORUM//DTD XHTML Mobile 1.0//EN" ||
         // Addition of the next two discussed in:
         // https://github.com/whatwg/html/issues/11810
         public_id == "-//WAPFORUM//DTD XHTML Mobile 1.1//EN" ||
         public_id == "-//WAPFORUM//DTD XHTML Mobile 1.2//EN";
}

}  // namespace blink
