#include "third_party/blink/renderer/core/html/parser/patch.h"

#include "base/memory/stack_allocated.h"
#include "base/types/pass_key.h"
#include "third_party/blink/renderer/core/dom/container_node.h"
#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/dom/document_fragment.h"
#include "third_party/blink/renderer/core/dom/element.h"
#include "third_party/blink/renderer/core/dom/events/event.h"
#include "third_party/blink/renderer/core/dom/node_traversal.h"
#include "third_party/blink/renderer/core/dom/processing_instruction.h"
#include "third_party/blink/renderer/core/dom/shadow_root.h"
#include "third_party/blink/renderer/core/event_type_names.h"
#include "third_party/blink/renderer/core/execution_context/execution_context.h"
#include "third_party/blink/renderer/core/html/cross_origin_attribute.h"
#include "third_party/blink/renderer/core/html/html_template_element.h"
#include "third_party/blink/renderer/core/html/parser/external_patch_loader.h"
#include "third_party/blink/renderer/core/html/parser/html_construction_site.h"
#include "third_party/blink/renderer/core/html/parser/html_document_parser.h"
#include "third_party/blink/renderer/core/html/parser/text_resource_decoder.h"
#include "third_party/blink/renderer/core/html_names.h"
#include "third_party/blink/renderer/platform/heap/collection_support/heap_vector.h"
#include "third_party/blink/renderer/platform/loader/fetch/fetch_parameters.h"
#include "third_party/blink/renderer/platform/loader/fetch/resource_fetcher.h"
#include "third_party/blink/renderer/platform/loader/subresource_integrity.h"
#include "third_party/blink/renderer/platform/runtime_enabled_features.h"
#include "third_party/blink/renderer/platform/wtf/text/atomic_string.h"
#include "third_party/blink/renderer/platform/wtf/text/text_encoding.h"

namespace blink {

namespace {

class NodeRemovalScope {
  STACK_ALLOCATED();

 public:
  void Remove(Node* node) { nodes_to_remove.push_back(node); }

  ~NodeRemovalScope() {
    for (Node* node : nodes_to_remove) {
      node->remove();
    }
  }

 private:
  HeapVector<Member<Node>> nodes_to_remove;
};

}  // namespace

Patch* Patch::Prepare(ContainerNode* scope,
                      const AtomicString& marker_name,
                      HTMLTemplateElement* template_element) {
  if (!RuntimeEnabledFeatures::DocumentPatchingEnabled() ||
      marker_name.IsNull()) {
    return nullptr;
  }

  while (auto* parent_template = DynamicTo<HTMLTemplateElement>(scope)) {
    if (auto* parent_patch = parent_template->GetPatch()) {
      if (!parent_patch->is_buffered()) {
        scope = parent_patch->parent_;
        continue;
      }
    }
    scope = parent_template->InsertionTarget();
  }

  const bool is_buffered =
      template_element &&
      (RuntimeEnabledFeatures::DeclarativeFragmentEnabled() &&
       (template_element->FastHasAttribute(html_names::kBufferAttr) ||
        template_element->FastHasAttribute(html_names::kIntegrityAttr)));

  ContainerNode* parent = nullptr;
  Node* start_marker = nullptr;
  Node* end_marker = nullptr;

  if (marker_name.empty()) {
    if (RuntimeEnabledFeatures::DeclarativeFragmentEnabled()) {
      parent = scope;
      start_marker = template_element;
      end_marker = template_element;
    }
  } else {
    if (scope == scope->GetDocument().body()) {
      scope = scope->GetDocument().documentElement();
    }

    DEFINE_STATIC_LOCAL(AtomicString, kNamePseudoAttr, ("name"));
    DEFINE_STATIC_LOCAL(AtomicString, kMarkerTarget, ("marker"));
    DEFINE_STATIC_LOCAL(AtomicString, kStartTarget, ("start"));
    DEFINE_STATIC_LOCAL(AtomicString, kEndTarget, ("end"));

    for (Node& descendant : NodeTraversal::DescendantsOf(*scope)) {
      auto* processing_instruction =
          DynamicTo<ProcessingInstruction>(descendant);
      if (!processing_instruction ||
          (processing_instruction->GetAttributeValue(
               kNamePseudoAttr, g_empty_atom) != marker_name)) {
        continue;
      }
      if (processing_instruction->target() == kMarkerTarget) {
        parent = processing_instruction->parentNode();
        start_marker = processing_instruction;
        end_marker = processing_instruction;
        break;
      }

      if (processing_instruction->target() != kStartTarget) {
        continue;
      }

      ContainerNode* pi_parent = processing_instruction->parentNode();
      int marker_depth = 0;
      NodeRemovalScope remove_scope;

      for (Node* node = processing_instruction->nextSibling(); node;
           node = node->nextSibling()) {
        if (ProcessingInstruction* next_processing_instruction =
                DynamicTo<ProcessingInstruction>(*node)) {
          if (next_processing_instruction->target() == kStartTarget) {
            marker_depth++;
          } else if (next_processing_instruction->target() == kEndTarget) {
            if (marker_depth == 0) {
              parent = pi_parent;
              start_marker = processing_instruction;
              end_marker = next_processing_instruction;
              break;
            }
            marker_depth--;
          }
        }

        remove_scope.Remove(node);
      }

      if (parent) {
        break;
      }

      // No end PI found.
      parent = pi_parent;
      start_marker = processing_instruction;
      end_marker = nullptr;
      break;
    }
  }

  if (!parent) {
    return nullptr;
  }

  return MakeGarbageCollected<Patch>(base::PassKey<Patch>(), parent,
                                     start_marker, end_marker, is_buffered,
                                     template_element);
}

void Patch::Apply(HTMLConstructionSite::InsertionLocation& location) {
  location.parent = parent_;
  location.next_child = end_marker_ && end_marker_->parentNode() == parent_
                            ? end_marker_
                            : nullptr;
}

void Patch::DidFinishParsingChildren(HTMLTemplateElement* template_element) {
  if (!loader_) {
    Finalize(template_element);
  }
}

void Patch::DidRemoveTemplateElement() {
  if (loader_) {
    loader_->Cancel();
  }
}

Patch::Patch(base::PassKey<Patch> key,
             ContainerNode* parent,
             Node* start_marker,
             Node* end_marker,
             bool is_buffered,
             HTMLTemplateElement* template_element)
    : parent_(parent),
      start_marker_(start_marker),
      end_marker_(end_marker),
      is_buffered_(is_buffered) {
  const AtomicString& src_attr =
      (template_element && RuntimeEnabledFeatures::DeclarativeFragmentEnabled())
          ? template_element->FastGetAttribute(html_names::kSrcAttr)
          : g_null_atom;
  if (!src_attr.empty()) {
    loader_ = MakeGarbageCollected<ExternalPatchLoader>(this, template_element,
                                                        src_attr);
  }
}

void Patch::Finalize(HTMLTemplateElement* template_element) {
  CHECK(RuntimeEnabledFeatures::DocumentPatchingEnabled());
  if (is_buffered_) {
    DocumentFragment* content = template_element->content();
    CHECK(content);
    CHECK(RuntimeEnabledFeatures::DeclarativeFragmentEnabled());
    if (parent_) {
      Node* next_child = end_marker_ && end_marker_->parentNode() == parent_
                             ? end_marker_.Get()
                             : nullptr;
      // TODO(nrosenthal): add more tests to assert script execution behavior at
      // this point.
      parent_->InsertBefore(content, next_child);
    }
  }

  if (ContainerNode* start_parent = start_marker_->parentNode()) {
    start_parent->ParserRemoveChild(*start_marker_);
  }
  if (end_marker_ && end_marker_ != start_marker_) {
    if (ContainerNode* end_parent = end_marker_->parentNode()) {
      end_parent->ParserRemoveChild(*end_marker_);
    }
  }

  if (template_element && template_element->parentNode()) {
    template_element->parentNode()->ParserRemoveChild(*template_element);
  }

  // In normal parsing, positional style invalidation and other effects happen
  // when finished parsing. When patching, we need to run the same logic when
  // the patch finalizes.
  if (Element* element = DynamicTo<Element>(*parent_)) {
    element->DidFinishParsingChildren();
  }
}

void Patch::Trace(Visitor* visitor) const {
  visitor->Trace(parent_);
  visitor->Trace(start_marker_);
  visitor->Trace(end_marker_);
  visitor->Trace(loader_);
}

}  // namespace blink
