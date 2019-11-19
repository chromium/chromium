// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/bindings/core/v8/script_source_code.h"

#include "third_party/blink/renderer/core/loader/resource/script_resource.h"
#include "third_party/blink/renderer/platform/loader/fetch/cached_metadata_handler.h"

namespace blink {

namespace {

ParkableString TreatNullSourceAsEmpty(const ParkableString& source) {
  // ScriptSourceCode allows for the representation of the null/not-there-really
  // ScriptSourceCode value.  Encoded by way of a source_.IsNull() being true,
  // with the nullary constructor to be used to construct such a value.
  //
  // Should the other constructors be passed a null string, that is interpreted
  // as representing the empty script. Consequently, we need to disambiguate
  // between such null string occurrences.  Do that by converting the latter
  // case's null strings into empty ones.
  if (source.IsNull())
    return ParkableString();

  return source;
}

KURL StripFragmentIdentifier(const KURL& url) {
  if (url.IsEmpty())
    return KURL();

  if (!url.HasFragmentIdentifier())
    return url;

  KURL copy = url;
  copy.RemoveFragmentIdentifier();
  return copy;
}

String SourceMapUrlFromResponse(const ResourceResponse& response) {
  String source_map_url = response.HttpHeaderField(http_names::kSourceMap);
  if (!source_map_url.IsEmpty())
    return source_map_url;

  // Try to get deprecated header.
  return response.HttpHeaderField(http_names::kXSourceMap);
}

}  // namespace

ScriptSourceCode::ScriptSourceCode(
    const ParkableString& source,
    ScriptSourceLocationType source_location_type,
    SingleCachedMetadataHandler* cache_handler,
    const KURL& url,
    const TextPosition& start_position)
    : source_(TreatNullSourceAsEmpty(source)),
      cache_handler_(cache_handler),
      not_streaming_reason_(ScriptStreamer::kInlineScript),
      url_(StripFragmentIdentifier(url)),
      start_position_(start_position),
      source_location_type_(source_location_type) {
  // External files should use a ScriptResource.
  DCHECK(source_location_type != ScriptSourceLocationType::kExternalFile);
}

ScriptSourceCode::ScriptSourceCode(
    const String& source,
    ScriptSourceLocationType source_location_type,
    SingleCachedMetadataHandler* cache_handler,
    const KURL& url,
    const TextPosition& start_position)
    : ScriptSourceCode(ParkableString(source.Impl()),
                       source_location_type,
                       cache_handler,
                       url,
                       start_position) {}

ScriptSourceCode::ScriptSourceCode(ScriptStreamer* streamer,
                                   ScriptResource* resource,
                                   ScriptStreamer::NotStreamingReason reason)
    : source_(TreatNullSourceAsEmpty(resource->SourceText())),
      cache_handler_(resource->CacheHandler()),
      streamer_(streamer),
      not_streaming_reason_(reason),
      url_(
          StripFragmentIdentifier(resource->GetResponse().CurrentRequestUrl())),
      source_map_url_(SourceMapUrlFromResponse(resource->GetResponse())),
      start_position_(TextPosition::MinimumPosition()),
      source_location_type_(ScriptSourceLocationType::kExternalFile) {
  DCHECK_EQ(!streamer, reason != ScriptStreamer::NotStreamingReason::kInvalid);
}

ScriptSourceCode::ScriptSourceCode(const String& source,
                                   SingleCachedMetadataHandler* cache_handler,
                                   const KURL& url)
    : source_(TreatNullSourceAsEmpty(ParkableString(source.Impl()))),
      cache_handler_(cache_handler),
      not_streaming_reason_(ScriptStreamer::kWorkerTopLevelScript),
      url_(url),
      start_position_(TextPosition::MinimumPosition()),
      source_location_type_(ScriptSourceLocationType::kUnknown) {}

ScriptSourceCode::~ScriptSourceCode() = default;

void ScriptSourceCode::Trace(blink::Visitor* visitor) {
  visitor->Trace(cache_handler_);
  visitor->Trace(streamer_);
}

}  // namespace blink
