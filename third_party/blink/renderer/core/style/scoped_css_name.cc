#include "third_party/blink/renderer/core/style/scoped_css_name.h"

#include "third_party/blink/renderer/core/dom/tree_scope.h"

namespace blink {

void ScopedCSSName::Trace(Visitor* visitor) const {
  visitor->Trace(tree_scope_);
}

void ScopedCSSNameList::Trace(Visitor* visitor) const {
  visitor->Trace(names_);
}

}  // namespace blink
