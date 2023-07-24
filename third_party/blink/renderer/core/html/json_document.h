#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_HTML_JSON_DOCUMENT_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_HTML_JSON_DOCUMENT_H_

#include "third_party/blink/renderer/core/html/html_document.h"
#include "third_party/blink/renderer/core/html/parser/html_document_parser.h"

namespace blink {
class JSONDocument : public HTMLDocument {
 public:
  JSONDocument(const DocumentInit&);

 private:
  DocumentParser* CreateParser() override;
};
}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_HTML_JSON_DOCUMENT_H_
