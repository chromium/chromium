// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_CSS_REMOTE_FONT_FACE_SOURCE_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_CSS_REMOTE_FONT_FACE_SOURCE_H_

#include "third_party/blink/renderer/core/css/css_font_face_source.h"
#include "third_party/blink/renderer/core/execution_context/security_context.h"
#include "third_party/blink/renderer/core/loader/resource/font_resource.h"
#include "third_party/blink/renderer/platform/wtf/allocator/allocator.h"

namespace blink {

class CSSFontFace;
class Document;
class FontSelector;
class FontCustomPlatformData;

class RemoteFontFaceSource final : public CSSFontFaceSource,
                                   public FontResourceClient {
  USING_PRE_FINALIZER(RemoteFontFaceSource, Dispose);

 public:
  enum Phase { kNoLimitExceeded, kShortLimitExceeded, kLongLimitExceeded };

  RemoteFontFaceSource(CSSFontFace*, FontSelector*, FontDisplay);
  ~RemoteFontFaceSource() override;
  void Dispose();

  bool IsLoading() const override;
  bool IsLoaded() const override;
  bool IsValid() const override;

  String GetURL() const override { return url_; }

  bool IsPendingDataUrl() const override;

  const FontCustomPlatformData* GetCustomPlaftormData() const override {
    return custom_font_data_.get();
  }

  void BeginLoadIfNeeded() override;
  void SetDisplay(FontDisplay) override;

  void NotifyFinished(Resource*) override;
  void FontLoadShortLimitExceeded(FontResource*) override;
  void FontLoadLongLimitExceeded(FontResource*) override;
  String DebugName() const override { return "RemoteFontFaceSource"; }

  bool IsInBlockPeriod() const override { return period_ == kBlockPeriod; }
  bool IsInFailurePeriod() const override { return period_ == kFailurePeriod; }

  // For UMA reporting
  bool HadBlankText() override { return histograms_.HadBlankText(); }
  void PaintRequested() override { histograms_.FallbackFontPainted(period_); }

  void Trace(Visitor*) const override;

 protected:
  scoped_refptr<SimpleFontData> CreateFontData(
      const FontDescription&,
      const FontSelectionCapabilities&) override;
  scoped_refptr<SimpleFontData> CreateLoadingFallbackFontData(
      const FontDescription&);

 private:
  // Periods of the Font Display Timeline.
  // https://drafts.csswg.org/css-fonts-4/#font-display-timeline
  // Note that kNotApplicablePeriod is an implementation detail indicating that
  // the font is loaded from memory cache synchronously, and hence, made
  // immediately available. As we never need to use a fallback for it, using
  // other DisplayPeriod values seem artificial. So we use a special value.
  enum DisplayPeriod {
    kBlockPeriod,
    kSwapPeriod,
    kFailurePeriod,
    kNotApplicablePeriod
  };

  class FontLoadHistograms {
    DISALLOW_NEW();

   public:
    // Should not change following order in CacheHitMetrics to be used for
    // metrics values.
    enum class CacheHitMetrics {
      kMiss,
      kDiskHit,
      kDataUrl,
      kMemoryHit,
      kMaxValue = kMemoryHit,
    };
    enum DataSource {
      kFromUnknown,
      kFromDataURL,
      kFromMemoryCache,
      kFromDiskCache,
      kFromNetwork
    };

    FontLoadHistograms()
        : blank_paint_time_recorded_(false),
          is_long_limit_exceeded_(false),
          data_source_(kFromUnknown) {}
    void LoadStarted();
    void FallbackFontPainted(DisplayPeriod);
    void LongLimitExceeded();
    void RecordFallbackTime();
    void RecordRemoteFont(const FontResource*);
    bool HadBlankText() { return !blank_paint_time_.is_null(); }
    DataSource GetDataSource() { return data_source_; }
    void MaySetDataSource(DataSource);

    static DataSource DataSourceForLoadFinish(const FontResource* font) {
      if (font->Url().ProtocolIsData())
        return kFromDataURL;
      return font->GetResponse().WasCached() ? kFromDiskCache : kFromNetwork;
    }

   private:
    void RecordLoadTimeHistogram(const FontResource*, base::TimeDelta duration);
    CacheHitMetrics DataSourceMetricsValue();
    base::TimeTicks load_start_time_;
    base::TimeTicks blank_paint_time_;
    // |blank_paint_time_recorded_| is used to prevent
    // WebFont.BlankTextShownTime to be reported incorrectly when the web font
    // fallbacks immediately. See https://crbug.com/591304
    bool blank_paint_time_recorded_;
    bool is_long_limit_exceeded_;
    DataSource data_source_;
  };

  Document* GetDocument() const;

  DisplayPeriod ComputeFontDisplayAutoPeriod() const;
  bool NeedsInterventionToAlignWithLCPGoal() const;

  DisplayPeriod ComputePeriod() const;
  bool UpdatePeriod() override;
  bool ShouldTriggerWebFontsIntervention();
  bool IsLowPriorityLoadingAllowedForRemoteFont() const override;
  FontDisplay GetFontDisplayWithDocumentPolicyCheck(FontDisplay,
                                                    const FontSelector*,
                                                    ReportOptions) const;

  // Our owning font face.
  Member<CSSFontFace> face_;
  Member<FontSelector> font_selector_;

  // |nullptr| if font is not loaded or failed to decode.
  scoped_refptr<FontCustomPlatformData> custom_font_data_;
  // |nullptr| if font is not loaded or failed to decode.
  String url_;

  FontDisplay display_;
  Phase phase_;
  DisplayPeriod period_;
  FontLoadHistograms histograms_;
  bool is_intervention_triggered_;
  bool finished_before_document_rendering_begin_;

  // Indicates whether FontData has been requested while the font is still being
  // loaded, in which case a fallback FontData is returned and used. If true, we
  // will render contents with fallback font, and later if we would switch to
  // the web font after it loads, there will be a layout shifting. Therefore, we
  // don't need to worry about layout shifting when it's false.
  bool has_been_requested_while_pending_;

  bool finished_before_lcp_limit_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_CSS_REMOTE_FONT_FACE_SOURCE_H_
