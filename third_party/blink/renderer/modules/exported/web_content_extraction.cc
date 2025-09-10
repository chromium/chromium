#include "third_party/blink/public/web/web_content_extraction.h"

#include "third_party/blink/public/platform/web_string.h"
#include "third_party/blink/public/web/web_local_frame.h"
#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/dom/node.h"
#include "third_party/blink/renderer/core/frame/local_frame.h"
#include "third_party/blink/renderer/modules/content_extraction/ai_page_content_agent.h"

namespace blink {

WebString DumpContentNodeTreeForTest(WebLocalFrame* web_frame) {
  DCHECK(web_frame);

  // We need to get the internal LocalFrame from the public WebLocalFrame.
  LocalFrame* frame =
      DynamicTo<LocalFrame>(WebLocalFrame::ToCoreFrame(*web_frame));
  DCHECK(frame);

  Document* document = frame->GetDocument();
  if (!document) {
    return WebString::FromUTF8("Error: no document.");
  }

  // AiPageContentAgent is a Supplement on Document.
  AIPageContentAgent* agent =
      AIPageContentAgent::GetOrCreateForTesting(*document);
  if (agent) {
    return WebString(agent->DumpContentNodeTreeForTest());
  }

  return WebString::FromUTF8("Error: no AiPageContentAgent");
}

WebString DumpContentNodeForTest(WebLocalFrame* web_frame, Node* node) {
  DCHECK(web_frame);
  DCHECK(node);

  // We need to get the internal LocalFrame from the public WebLocalFrame.
  LocalFrame* frame =
      DynamicTo<LocalFrame>(WebLocalFrame::ToCoreFrame(*web_frame));
  DCHECK(frame);

  Document* document = frame->GetDocument();
  if (!document) {
    return WebString::FromUTF8("Error: no document.");
  }

  // AiPageContentAgent is a Supplement on Document.
  AIPageContentAgent* agent =
      AIPageContentAgent::GetOrCreateForTesting(*document);
  if (agent) {
    return WebString(agent->DumpContentNodeForTest(node));
  }

  return WebString::FromUTF8("Error: no AiPageContentAgent");
}

}  // namespace blink
