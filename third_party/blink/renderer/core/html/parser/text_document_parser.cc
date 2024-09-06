/*
 * Copyright (C) 2010 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE COMPUTER, INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE COMPUTER, INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "third_party/blink/renderer/core/html/parser/text_document_parser.h"

#include "third_party/blink/renderer/core/html/parser/html_tree_builder.h"
#include "third_party/blink/renderer/core/html/parser/parser_synchronization_policy.h"
#include "third_party/blink/renderer/core/html_names.h"
#include "third_party/blink/renderer/core/keywords.h"

namespace blink {

TextDocumentParser::TextDocumentParser(HTMLDocument& document,
                                       ParserSynchronizationPolicy sync_policy)
    : HTMLDocumentParser(document, sync_policy, kDisallowPrefetching),
      have_inserted_fake_pre_element_(false) {}

TextDocumentParser::~TextDocumentParser() = default;

void TextDocumentParser::AppendBytes(base::span<const uint8_t> data) {
  if (data.empty() || IsStopped()) {
    return;
  }

  if (!have_inserted_fake_pre_element_)
    InsertFakePreElement();
  HTMLDocumentParser::AppendBytes(data);
}

void TextDocumentParser::InsertFakePreElement() {
  // In principle, we should create a specialized tree builder for
  // TextDocuments, but instead we re-use the existing HTMLTreeBuilder. We
  // create two fake tokens and pass them to the tree builder rather than
  // sending fake bytes through the front-end of the parser to avoid disturbing
  // the line/column number calculations.
  Vector<Attribute> attributes;

  // Allow the browser to display the text file in dark mode if it is set as
  // the preferred color scheme.
  attributes.push_back(
      Attribute(html_names::kNameAttr, keywords::kColorScheme));
  attributes.push_back(
      Attribute(html_names::kContentAttr, AtomicString("light dark")));
  AtomicHTMLToken fake_meta(HTMLToken::kStartTag, html_names::HTMLTag::kMeta,
                            attributes);
  TreeBuilder()->ConstructTree(&fake_meta);
  attributes.clear();

  // Wrap the actual contents of the text file in <pre>.
  attributes.push_back(
      Attribute(html_names::kStyleAttr,
                AtomicString("word-wrap: break-word; white-space: pre-wrap;")));
  AtomicHTMLToken fake_pre(HTMLToken::kStartTag, html_names::HTMLTag::kPre,
                           attributes);
  TreeBuilder()->ConstructTree(&fake_pre);

  // The document could have been detached by an extension while the
  // tree was being constructed.
  if (IsStopped())
    return;

  // Normally we would skip the first \n after a <pre> element, but we don't
  // want to skip the first \n for text documents!
  TreeBuilder()->SetShouldSkipLeadingNewline(false);

  // Although Text Documents expose a "pre" element in their DOM, they
  // act like a <plaintext> tag, so we have to force plaintext mode.
  ForcePlaintextForTextDocument();

  have_inserted_fake_pre_element_ = true;
}

}  // namespace blink
