/*
    Copyright (C) 2010 Rob Buis <rwlbuis@gmail.com>
    Copyright (C) 2011 Cosmin Truta <ctruta@gmail.com>
    Copyright (C) 2012 University of Szeged
    Copyright (C) 2012 Renata Hodovan <reni@webkit.org>

    This library is free software; you can redistribute it and/or
    modify it under the terms of the GNU Library General Public
    License as published by the Free Software Foundation; either
    version 2 of the License, or (at your option) any later version.

    This library is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
    Library General Public License for more details.

    You should have received a copy of the GNU Library General Public License
    along with this library; see the file COPYING.LIB.  If not, write to
    the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
    Boston, MA 02110-1301, USA.
*/

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_SVG_SVG_RESOURCE_DOCUMENT_CONTENT_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_SVG_SVG_RESOURCE_DOCUMENT_CONTENT_H_

#include "third_party/blink/renderer/core/core_export.h"
#include "third_party/blink/renderer/platform/heap/garbage_collected.h"
#include "third_party/blink/renderer/platform/heap/member.h"

namespace blink {

class Document;
class ExecutionContext;
class FetchParameters;
class KURL;
class ResourceClient;
class TextResource;

class CORE_EXPORT SVGResourceDocumentContent final
    : public GarbageCollected<SVGResourceDocumentContent> {
 public:
  static SVGResourceDocumentContent* Fetch(FetchParameters&,
                                           Document&,
                                           ResourceClient*);

  SVGResourceDocumentContent(TextResource* resource, ExecutionContext* context)
      : resource_(resource), context_(context) {
    DCHECK(resource_);
    DCHECK(context_);
  }

  Document* GetDocument();
  const KURL& Url() const;

  bool IsLoading() const;

  void Trace(Visitor*) const;

 private:
  void SetWasRevalidating() { was_revalidating_ = true; }

  Member<TextResource> resource_;
  Member<Document> document_;
  Member<ExecutionContext> context_;
  bool was_revalidating_ = false;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_SVG_SVG_RESOURCE_DOCUMENT_CONTENT_H_
