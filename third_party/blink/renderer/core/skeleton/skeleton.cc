// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/skeleton/skeleton.h"

#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/dom/document_init.h"
#include "third_party/blink/renderer/core/keywords.h"
#include "third_party/blink/renderer/platform/wtf/text/string_builder.h"

namespace blink {

void Skeleton::Render(KURL url, Document& owner_document) {
  ExecutionContext* context = owner_document.GetExecutionContext();
  skeleton_document_ = DocumentInit::Create()
                           .WithTypeFrom(keywords::kTextHtml)
                           .WithExecutionContext(context)
                           .WithAgent(*context->GetAgent())
                           .CreateDocument();
  skeleton_document_->setAllowDeclarativeShadowRoots(true);
  skeleton_document_->SetMimeType(keywords::kTextHtml);

  // Generate a placeholder skeleton document before we have resource loading
  // and parsing in place.
  GenerateSkeleton(url);

  observer_->DocumentReady(*this);
}

void Skeleton::GenerateSkeleton(KURL url) {
  StringBuilder builder;
  builder.Append(R"HTML(
    <html>
      <style>
        html {
          background: teal;
          color: white;
          overflow: hidden;
          position: fixed;
          inset: 0;
        }
        main { font-size: 32px; }
      </style>
      <body>
        <main>Skeleton for:
  )HTML");

  builder.Append(url.GetString());

  builder.Append(R"HTML(
        </main>
      </body>
    </html>
  )HTML");

  CHECK(skeleton_document_);
  skeleton_document_->SetContent(builder.ToString());
}

void Skeleton::Trace(Visitor* visitor) const {
  visitor->Trace(observer_);
  visitor->Trace(skeleton_document_);
}

}  // namespace blink
