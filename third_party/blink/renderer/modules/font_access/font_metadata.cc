// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/font_access/font_metadata.h"

#include <memory>
#include <utility>

#include "base/metrics/histogram_functions.h"
#include "base/numerics/safe_conversions.h"
#include "build/build_config.h"
#include "third_party/blink/renderer/bindings/core/v8/script_promise_resolver.h"
#include "third_party/blink/renderer/core/fileapi/blob.h"
#include "third_party/blink/renderer/platform/bindings/script_state.h"
#include "third_party/blink/renderer/platform/fonts/font_cache.h"
#include "third_party/blink/renderer/platform/fonts/font_global_context.h"
#include "third_party/blink/renderer/platform/fonts/font_unique_name_lookup.h"
#include "third_party/blink/renderer/platform/scheduler/public/thread.h"
#include "third_party/skia/include/core/SkStream.h"
#include "third_party/skia/include/core/SkTypes.h"

namespace blink {

namespace {

// Sets up internal FontUniqueLookup data that will allow matching unique names,
// on platforms that apply.
void SetUpFontUniqueLookupIfNecessary() {
  FontUniqueNameLookup* unique_name_lookup =
      FontGlobalContext::Get().GetFontUniqueNameLookup();
  if (!unique_name_lookup)
    return;
  // Contrary to what the method name might imply, this is not an idempotent
  // method. It also initializes state in the FontUniqueNameLookup object.
  unique_name_lookup->IsFontUniqueNameLookupReadyForSyncLookup();
}

}  // namespace

FontMetadata::FontMetadata(const FontEnumerationEntry& entry)
    : postscript_name_(entry.postscript_name),
      full_name_(entry.full_name),
      family_(entry.family),
      style_(entry.style) {}

ScriptPromise<Blob> FontMetadata::blob(ScriptState* script_state) {
  auto* resolver =
      MakeGarbageCollected<ScriptPromiseResolver<Blob>>(script_state);
  auto promise = resolver->Promise();

  ExecutionContext::From(script_state)
      ->GetTaskRunner(TaskType::kFontLoading)
      ->PostTask(FROM_HERE,
                 BindOnce(&FontMetadata::BlobImpl, WrapPersistent(resolver),
                          postscript_name_));

  return promise;
}

// static
void FontMetadata::BlobImpl(ScriptPromiseResolver<Blob>* resolver,
                            const String& postscript_name) {
  if (!resolver->GetScriptState()->ContextIsValid())
    return;

  SetUpFontUniqueLookupIfNecessary();

  FontDescription description;
  const SimpleFontData* font_data =
      FontCache::Get().GetFontData(description, AtomicString(postscript_name),
                                   AlternateFontName::kLocalUniqueFace);
  if (!font_data) {
    ScriptState::Scope scope(resolver->GetScriptState());
    resolver->Reject(V8ThrowException::CreateTypeError(
        resolver->GetScriptState()->GetIsolate(),
        StrCat({"The font ", postscript_name, " could not be accessed."})));
    return;
  }

  const SkTypeface* typeface = font_data->PlatformData().Typeface();

  // On Mac, this will not be as efficient as on other platforms: data from
  // tables will be copied and assembled into valid SNFT font data. This is
  // because Mac system APIs only return per-table data.
  int ttc_index = 0;
  std::unique_ptr<SkStreamAsset> stream = typeface->openStream(&ttc_index);

  if (!(stream && stream->getMemoryBase())) {
    // TODO(https://crbug.com/1086840): openStream rarely fails, but it happens
    // sometimes. A potential remediation is to synthesize a font from tables
    // at the cost of memory and throughput.
    ScriptState::Scope scope(resolver->GetScriptState());
    resolver->Reject(V8ThrowException::CreateTypeError(
        resolver->GetScriptState()->GetIsolate(),
        StrCat(
            {"Font data for ", postscript_name, " could not be accessed."})));
    return;
  }

  wtf_size_t font_byte_size =
      base::checked_cast<wtf_size_t>(stream->getLength());

  // TODO(https://crbug.com/1069900): This copies the font bytes. Lazy load and
  // stream the data instead.
  Vector<uint8_t> bytes(font_byte_size);
  size_t returned_size = stream->read(bytes.data(), font_byte_size);
  DCHECK_EQ(returned_size, font_byte_size);

  scoped_refptr<RawData> raw_data = RawData::Create();
  bytes.swap(raw_data->MutableData());
  auto blob_data = std::make_unique<BlobData>();
  blob_data->AppendData(std::move(raw_data));
  blob_data->SetContentType("application/octet-stream");

  auto* blob = MakeGarbageCollected<Blob>(
      BlobDataHandle::Create(std::move(blob_data), font_byte_size));
  resolver->Resolve(blob);
}

}  // namespace blink
