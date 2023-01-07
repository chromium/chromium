// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_MODULES_DOCUMENT_PICTURE_IN_PICTURE_DOCUMENT_PICTURE_IN_PICTURE_H_
#define THIRD_PARTY_BLINK_RENDERER_MODULES_DOCUMENT_PICTURE_IN_PICTURE_DOCUMENT_PICTURE_IN_PICTURE_H_

#include "third_party/blink/renderer/modules/modules_export.h"
#include "third_party/blink/renderer/platform/bindings/script_wrappable.h"
#include "third_party/blink/renderer/platform/heap/garbage_collected.h"
#include "third_party/blink/renderer/platform/supplementable.h"
#include "third_party/blink/renderer/platform/wtf/allocator/allocator.h"

namespace blink {

class DocumentPictureInPictureOptions;
class DocumentPictureInPictureSession;
class ExceptionState;
class ExecutionContext;
class Navigator;
class ScriptPromise;
class ScriptState;

class MODULES_EXPORT DocumentPictureInPicture : public ScriptWrappable,
                                                public Supplement<Navigator> {
  DEFINE_WRAPPERTYPEINFO();

 public:
  static DocumentPictureInPicture* From(ExecutionContext* execution_context,
                                        Navigator& navigator);

  DocumentPictureInPicture(ExecutionContext*, Navigator&);

  ScriptPromise requestWindow(ScriptState*,
                              DocumentPictureInPictureOptions*,
                              ExceptionState&);

  DocumentPictureInPictureSession* session(ScriptState*) const;

  static DocumentPictureInPicture* documentPictureInPicture(ScriptState*,
                                                            Navigator&);

  static const char kSupplementName[];

  void Trace(Visitor*) const override;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_MODULES_DOCUMENT_PICTURE_IN_PICTURE_DOCUMENT_PICTURE_IN_PICTURE_H_
