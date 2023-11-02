/**
 * Copyright (C) 2001-2002 Thomas Broyer, Charlie Bozeman and Daniel Veillard.
 * Copyright (C) 2007 Alexey Proskuryakov <ap@webkit.org>
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is fur-
 * nished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FIT-
 * NESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.  IN NO EVENT SHALL THE
 * AUTHORS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER
 * IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CON-
 * NECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 *
 * Except as contained in this notice, the name of the authors shall not
 * be used in advertising or otherwise to promote the sale, use or other deal-
 * ings in this Software without prior written authorization from him.
 */

#include "third_party/blink/renderer/core/xml/xslt_extensions.h"

#include <libxml/xpathInternals.h>
#include <libxslt/extensions.h>
#include <libxslt/extra.h>
#include <libxslt/xsltutils.h>

#include "base/check.h"

namespace blink {

// FIXME: This code is taken from libexslt 1.1.11; should sync with newer
// versions.
static void ExsltNodeSetFunction(xmlXPathParserContextPtr ctxt, int nargs) {
  xmlChar* strval;
  xmlNodePtr ret_node;
  xmlXPathObjectPtr ret;

  if (nargs != 1) {
    xmlXPathSetArityError(ctxt);
    return;
  }

  if (xmlXPathStackIsNodeSet(ctxt)) {
    xsltFunctionNodeSet(ctxt, nargs);
    return;
  }

  // node-set can also take a string and turn it into a singleton node
  // set with one text node. This may null-deref if allocating the
  // document, text node, etc. fails; that behavior is expected.

  // Create a document to hold the text node result.
  xsltTransformContextPtr tctxt = xsltXPathGetTransformContext(ctxt);
  xmlDocPtr fragment = xsltCreateRVT(tctxt);
  xsltRegisterLocalRVT(tctxt, fragment);

  // Create the text node and wrap it in a result set.
  strval = xmlXPathPopString(ctxt);
  ret_node = xmlNewDocText(fragment, strval);
  xmlAddChild(reinterpret_cast<xmlNodePtr>(fragment), ret_node);
  ret = xmlXPathNewNodeSet(ret_node);
  CHECK(ret);

  if (strval)
    xmlFree(strval);

  valuePush(ctxt, ret);
}

void RegisterXSLTExtensions(xsltTransformContextPtr ctxt) {
  xsltRegisterExtFunction(ctxt, (const xmlChar*)"node-set",
                          (const xmlChar*)"http://exslt.org/common",
                          ExsltNodeSetFunction);
}

}  // namespace blink
