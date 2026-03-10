#include "third_party/blink/renderer/core/timing/timing_utils.h"

#include <algorithm>

#include "third_party/blink/renderer/core/dom/element.h"
#include "third_party/blink/renderer/core/dom/events/event_path.h"
#include "third_party/blink/renderer/core/dom/events/event_target.h"
#include "third_party/blink/renderer/core/dom/node.h"
#include "third_party/blink/renderer/core/dom/shadow_root.h"
#include "third_party/blink/renderer/core/exported/web_plugin_container_impl.h"
#include "third_party/blink/renderer/core/html_names.h"
#include "third_party/blink/renderer/platform/wtf/text/string_builder.h"
#include "third_party/blink/renderer/platform/wtf/vector.h"

namespace blink {

namespace {

void AppendElementSelector(StringBuilder& builder, Element& element) {
  builder.Append(element.nodeName());
  if (element.HasID()) {
    builder.Append("#");
    builder.Append(element.GetIdAttribute());
  } else if (element.hasAttribute(html_names::kSrcAttr)) {
    builder.Append("[src=\"");
    builder.Append(element.getAttribute(html_names::kSrcAttr));
    builder.Append("\"]");
  }
}

}  // namespace

// Returns a string representation of an EventTarget, suitable for including in
// performance timeline entries.
// This may be used for non-UI events and is currently used by the Long
// Animation Frames (LoAF) API.
AtomicString EventTargetToString(EventTarget* target) {
  if (!target) {
    return g_null_atom;
  }
  if (Node* node = target->ToNode()) {
    if (Element* element = DynamicTo<Element>(node)) {
      StringBuilder builder;
      AppendElementSelector(builder, *element);
      return builder.ToAtomicString();
    }
    return AtomicString(node->nodeName());
  }
  return target->InterfaceName();
}

// Returns a string representation of an EventPath (CSS selector style).
// This is primarily intended for UI events where the propagation path
// provides meaningful context about the interaction.
AtomicString EventPathToSelector(const EventPath& path) {
  if (path.IsEmpty()) {
    return g_null_atom;
  }

  // We want to build the selector from the target up to a reasonable depth.
  // The EventPath is ordered from target to window.
  constexpr int kMaxDepth = 10;
  int depth = 0;

  Vector<String, kMaxDepth> components;

  for (const auto& context : path.NodeEventContexts()) {
    Node* node = &context.GetNode();
    if (auto* element = DynamicTo<Element>(node)) {
      StringBuilder element_builder;
      AppendElementSelector(element_builder, *element);
      components.push_back(element_builder.ToString());

      if (++depth >= kMaxDepth || element->HasID()) {
        break;
      }
    } else if (IsA<ShadowRoot>(node)) {
      // We generally stop at shadow boundaries for the string representation
      // to avoid leaking internal structure, similar to composedPath().
      break;
    }
  }

  if (components.empty()) {
    return g_null_atom;
  }

  // EventPath is from target to root, but we want root to target.
  std::reverse(components.begin(), components.end());

  StringBuilder builder;
  builder.AppendRange(components, " > ");
  return builder.ToAtomicString();
}

}  // namespace blink
