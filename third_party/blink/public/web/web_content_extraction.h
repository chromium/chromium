#ifndef THIRD_PARTY_BLINK_PUBLIC_WEB_WEB_CONTENT_EXTRACTION_H_
#define THIRD_PARTY_BLINK_PUBLIC_WEB_WEB_CONTENT_EXTRACTION_H_

#include "third_party/blink/public/platform/web_common.h"
#include "third_party/blink/public/platform/web_string.h"

namespace blink {

class WebLocalFrame;

// Retrieves a dump of the content node tree for the given frame
// and its local frame descendants.
BLINK_EXPORT WebString DumpContentNodeTreeForTest(WebLocalFrame* frame);

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_PUBLIC_WEB_WEB_CONTENT_EXTRACTION_H_
