// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "printing/common/metafile_utils.h"

#include "base/time/time.h"
#include "printing/common/printing_features.h"
#include "third_party/skia/include/core/SkCanvas.h"
#include "third_party/skia/include/core/SkPicture.h"
#include "third_party/skia/include/core/SkPictureRecorder.h"
#include "third_party/skia/include/core/SkTime.h"
#include "third_party/skia/include/docs/SkPDFDocument.h"

namespace {

SkTime::DateTime TimeToSkTime(base::Time time) {
  base::Time::Exploded exploded;
  time.UTCExplode(&exploded);
  SkTime::DateTime skdate;
  skdate.fTimeZoneMinutes = 0;
  skdate.fYear = exploded.year;
  skdate.fMonth = exploded.month;
  skdate.fDayOfWeek = exploded.day_of_week;
  skdate.fDay = exploded.day_of_month;
  skdate.fHour = exploded.hour;
  skdate.fMinute = exploded.minute;
  skdate.fSecond = exploded.second;
  return skdate;
}

sk_sp<SkPicture> GetEmptyPicture() {
  SkPictureRecorder rec;
  SkCanvas* canvas = rec.beginRecording(100, 100);
  // Add some ops whose net effects equal to a noop.
  canvas->save();
  canvas->restore();
  return rec.finishRecordingAsPicture();
}

}  // namespace

namespace printing {

sk_sp<SkDocument> MakePdfDocument(const std::string& creator,
                                  SkWStream* stream) {
  SkPDF::Metadata metadata;
  SkTime::DateTime now = TimeToSkTime(base::Time::Now());
  metadata.fCreation = now;
  metadata.fModified = now;
  metadata.fCreator = creator.empty()
                          ? SkString("Chromium")
                          : SkString(creator.c_str(), creator.size());
  metadata.fRasterDPI = 300.0f;
  metadata.fSubsetter =
      base::FeatureList::IsEnabled(printing::features::kHarfBuzzPDFSubsetter)
          ? SkPDF::Metadata::kHarfbuzz_Subsetter
          : SkPDF::Metadata::kSfntly_Subsetter;
  return SkPDF::MakeDocument(stream, metadata);
}

sk_sp<SkData> SerializeOopPicture(SkPicture* pic, void* ctx) {
  const ContentToProxyIdMap* context =
      reinterpret_cast<const ContentToProxyIdMap*>(ctx);
  uint32_t pic_id = pic->uniqueID();
  auto iter = context->find(pic_id);
  if (iter == context->end())
    return nullptr;

  return SkData::MakeWithCopy(&pic_id, sizeof(pic_id));
}

sk_sp<SkPicture> DeserializeOopPicture(const void* data,
                                       size_t length,
                                       void* ctx) {
  uint32_t pic_id;
  if (length < sizeof(pic_id)) {
    NOTREACHED();  // Should not happen if the content is as written.
    return GetEmptyPicture();
  }
  memcpy(&pic_id, data, sizeof(pic_id));

  auto* context = reinterpret_cast<DeserializationContext*>(ctx);
  auto iter = context->find(pic_id);
  if (iter == context->end() || !iter->second) {
    // When we don't have the out-of-process picture available, we return
    // an empty picture. Returning a nullptr will cause the deserialization
    // crash.
    return GetEmptyPicture();
  }
  return iter->second;
}

SkSerialProcs SerializationProcs(SerializationContext* ctx) {
  SkSerialProcs procs;
  procs.fPictureProc = SerializeOopPicture;
  procs.fPictureCtx = ctx;
  return procs;
}

SkDeserialProcs DeserializationProcs(DeserializationContext* ctx) {
  SkDeserialProcs procs;
  procs.fPictureProc = DeserializeOopPicture;
  procs.fPictureCtx = ctx;
  return procs;
}

}  // namespace printing
