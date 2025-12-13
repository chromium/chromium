#include "third_party/blink/renderer/core/timing/timing_utils.h"

#include "third_party/blink/renderer/core/dom/element.h"
#include "third_party/blink/renderer/core/dom/events/event_target.h"
#include "third_party/blink/renderer/core/dom/node.h"
#include "third_party/blink/renderer/core/exported/web_plugin_container_impl.h"
#include "third_party/blink/renderer/core/html_names.h"
#include "third_party/blink/renderer/platform/wtf/text/string_builder.h"

namespace blink {

AtomicString EventTargetToString(EventTarget* target) {
  if (!target) {
    return g_null_atom;
  }
  if (Node* node = target->ToNode()) {
    StringBuilder builder;
    builder.Append(node->nodeName());
    if (Element* element = DynamicTo<Element>(node)) {
      if (element->HasID()) {
        builder.Append("#");
        builder.Append(element->GetIdAttribute());
      } else if (element->hasAttribute(html_names::kSrcAttr)) {
        // TODO(crbug.com/40887145): Remove this kill switch after feature roll
        // out, since it's just guarding a tiny compat change.
        if (RuntimeEnabledFeatures::
                EventTargetStringIdentifierUsesQuotesEnabled()) {
          builder.Append("[src=\"");
          builder.Append(element->getAttribute(html_names::kSrcAttr));
          builder.Append("\"]");
        } else {
          builder.Append("[src=");
          builder.Append(element->getAttribute(html_names::kSrcAttr));
          builder.Append("]");
        }
      }
    }
    return builder.ToAtomicString();
  }
  return target->InterfaceName();
}

}  // namespace blink
