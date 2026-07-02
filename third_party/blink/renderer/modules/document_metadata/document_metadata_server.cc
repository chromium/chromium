// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/document_metadata/document_metadata_server.h"

#include <algorithm>

#include "third_party/blink/renderer/core/frame/local_dom_window.h"
#include "third_party/blink/renderer/core/frame/local_frame.h"
#include "third_party/blink/renderer/modules/document_metadata/document_metadata_extractor.h"
#include "third_party/blink/renderer/platform/text/text_break_iterator.h"
#include "third_party/blink/renderer/platform/wtf/hash_map.h"
#include "third_party/blink/renderer/platform/wtf/text/string_hash.h"
#include "third_party/blink/renderer/platform/wtf/text/string_view.h"

namespace blink {

namespace {

enum class KeywordType { kAllowed, kBlocked };

void MatchKeywords(const String& text,
                   const HashMap<String, KeywordType>& keywords,
                   bool& has_allowed,
                   bool& has_blocked) {
  if (text.empty()) {
    return;
  }

  TextBreakIterator* iter = WordBreakIterator(text);
  if (!iter) {
    return;
  }

  int32_t pos = iter->first();
  int32_t next = iter->next();
  while (next != icu::BreakIterator::DONE) {
    if (IsWordTextBreak(iter)) {
      String word = text.substr(pos, next - pos).FoldCase();
      auto it = keywords.find(word);
      if (it != keywords.end()) {
        if (it->value == KeywordType::kAllowed) {
          has_allowed = true;
        } else {
          has_blocked = true;
        }
      }
    }
    pos = next;
    next = iter->next();
  }
}

}  // namespace

// static
const char DocumentMetadataServer::kSupplementName[] = "DocumentMetadataServer";

// static
DocumentMetadataServer* DocumentMetadataServer::From(Document& document) {
  return Supplement<Document>::From<DocumentMetadataServer>(document);
}

// static
void DocumentMetadataServer::BindReceiver(
    LocalFrame* frame,
    mojo::PendingReceiver<mojom::blink::DocumentMetadata> receiver) {
  DCHECK(frame && frame->GetDocument());
  auto& document = *frame->GetDocument();
  auto* server = DocumentMetadataServer::From(document);
  if (!server) {
    server = MakeGarbageCollected<DocumentMetadataServer>(
        base::PassKey<DocumentMetadataServer>(), *frame);
    Supplement<Document>::ProvideTo(document, server);
  }
  server->Bind(std::move(receiver));
}

DocumentMetadataServer::DocumentMetadataServer(
    base::PassKey<DocumentMetadataServer>,
    LocalFrame& frame)
    : Supplement<Document>(*frame.GetDocument()),
      receiver_(this, frame.DomWindow()) {}

void DocumentMetadataServer::Bind(
    mojo::PendingReceiver<mojom::blink::DocumentMetadata> receiver) {
  // We expect the interface to be bound at most once when the page is loaded
  // to service the GetEntities() call.
  receiver_.reset();
  // See https://bit.ly/2S0zRAS for task types.
  receiver_.Bind(std::move(receiver), GetSupplementable()->GetTaskRunner(
                                          TaskType::kMiscPlatformAPI));
}

void DocumentMetadataServer::Trace(Visitor* visitor) const {
  visitor->Trace(receiver_);
  Supplement<Document>::Trace(visitor);
}

void DocumentMetadataServer::GetEntities(GetEntitiesCallback callback) {
  std::move(callback).Run(
      DocumentMetadataExtractor::Extract(*GetSupplementable()));
}

void DocumentMetadataServer::ClassifyProductDetails(
    const Vector<String>& allowed_keywords,
    const Vector<String>& blocked_keywords,
    ClassifyProductDetailsCallback callback) {
  mojom::blink::WebPagePtr page =
      DocumentMetadataExtractor::Extract(*GetSupplementable());
  if (!page || page->entities.empty()) {
    std::move(callback).Run(nullptr);
    return;
  }

  HashMap<String, KeywordType> keywords_map;
  keywords_map.ReserveCapacityForSize(allowed_keywords.size() +
                                      blocked_keywords.size());
  for (const auto& kw : allowed_keywords) {
    keywords_map.insert(kw.FoldCase(), KeywordType::kAllowed);
  }
  for (const auto& kw : blocked_keywords) {
    keywords_map.insert(kw.FoldCase(), KeywordType::kBlocked);
  }

  const bool has_product_group = std::ranges::any_of(
      page->entities,
      [](const auto& entity) { return entity->type == "ProductGroup"; });

  const StringView target_type = has_product_group ? "ProductGroup" : "Product";
  mojom::blink::ProductClassificationResultPtr result;

  for (const auto& entity : page->entities) {
    if (entity->type == target_type) {
      if (!result) {
        result = mojom::blink::ProductClassificationResult::New();
        result->allowed_keyword_found = false;
        result->blocked_keyword_found = false;
      }

      for (const auto& prop : entity->properties) {
        if ((prop->name == "name" || prop->name == "description") &&
            prop->values->is_string_values()) {
          for (const auto& str : prop->values->get_string_values()) {
            MatchKeywords(str, keywords_map, result->allowed_keyword_found,
                          result->blocked_keyword_found);
          }
        }
      }
    }
  }

  std::move(callback).Run(std::move(result));
}

}  // namespace blink
