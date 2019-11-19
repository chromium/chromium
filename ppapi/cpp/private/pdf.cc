// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ppapi/cpp/private/pdf.h"

#include "ppapi/c/trusted/ppb_browser_font_trusted.h"
#include "ppapi/cpp/image_data.h"
#include "ppapi/cpp/instance_handle.h"
#include "ppapi/cpp/module_impl.h"
#include "ppapi/cpp/var.h"

namespace pp {

namespace {

template <> const char* interface_name<PPB_PDF>() {
  return PPB_PDF_INTERFACE;
}

void ConvertPrivateAccessibilityTextStyleInfo(
    const PDF::PrivateAccessibilityTextStyleInfo& text_style,
    PP_PrivateAccessibilityTextStyleInfo* info) {
  info->font_name = text_style.font_name.c_str();
  info->font_name_length = text_style.font_name.size();
  info->font_weight = text_style.font_weight;
  info->render_mode = text_style.render_mode;
  info->font_size = text_style.font_size;
  info->fill_color = text_style.fill_color;
  info->stroke_color = text_style.stroke_color;
  info->is_italic = text_style.is_italic;
  info->is_bold = text_style.is_bold;
}

void ConvertPrivateAccessibilityTextRunInfo(
    const PDF::PrivateAccessibilityTextRunInfo& text_run,
    PP_PrivateAccessibilityTextRunInfo* info) {
  info->len = text_run.len;
  info->bounds = text_run.bounds;
  info->direction = text_run.direction;
  ConvertPrivateAccessibilityTextStyleInfo(text_run.style, &info->style);
}

void ConvertPrivateAccessibilityLinkInfo(
    const PDF::PrivateAccessibilityLinkInfo& link,
    PP_PrivateAccessibilityLinkInfo* info) {
  info->url = link.url.c_str();
  info->url_length = link.url.size();
  info->index_in_page = link.index_in_page;
  info->text_run_index = link.text_run_index;
  info->text_run_count = link.text_run_count;
  info->bounds = link.bounds;
}

void ConvertPrivateAccessibilityImageInfo(
    const PDF::PrivateAccessibilityImageInfo& image,
    PP_PrivateAccessibilityImageInfo* info) {
  info->alt_text = image.alt_text.c_str();
  info->alt_text_length = image.alt_text.size();
  info->text_run_index = image.text_run_index;
  info->bounds = image.bounds;
}

}  // namespace

// static
bool PDF::IsAvailable() {
  return has_interface<PPB_PDF>();
}

// static
PP_Resource PDF::GetFontFileWithFallback(
    const InstanceHandle& instance,
    const PP_BrowserFont_Trusted_Description* description,
    PP_PrivateFontCharset charset) {
  if (has_interface<PPB_PDF>()) {
    return get_interface<PPB_PDF>()->GetFontFileWithFallback(
        instance.pp_instance(), description, charset);
  }
  return 0;
}

// static
bool PDF::GetFontTableForPrivateFontFile(PP_Resource font_file,
                                         uint32_t table,
                                         void* output,
                                         uint32_t* output_length) {
  if (has_interface<PPB_PDF>()) {
    return get_interface<PPB_PDF>()->GetFontTableForPrivateFontFile(font_file,
        table, output, output_length);
  }
  return false;
}

// static
void PDF::SearchString(const InstanceHandle& instance,
                       const unsigned short* string,
                       const unsigned short* term,
                       bool case_sensitive,
                       PP_PrivateFindResult** results,
                       uint32_t* count) {
  if (has_interface<PPB_PDF>()) {
    get_interface<PPB_PDF>()->SearchString(instance.pp_instance(), string,
        term, case_sensitive, results, count);
  }
}

// static
void PDF::DidStartLoading(const InstanceHandle& instance) {
  if (has_interface<PPB_PDF>())
    get_interface<PPB_PDF>()->DidStartLoading(instance.pp_instance());
}

// static
void PDF::DidStopLoading(const InstanceHandle& instance) {
  if (has_interface<PPB_PDF>())
    get_interface<PPB_PDF>()->DidStopLoading(instance.pp_instance());
}

// static
void PDF::SetContentRestriction(const InstanceHandle& instance,
                                int restrictions) {
  if (has_interface<PPB_PDF>()) {
    get_interface<PPB_PDF>()->SetContentRestriction(instance.pp_instance(),
                                                    restrictions);
  }
}

// static
void PDF::UserMetricsRecordAction(const InstanceHandle& instance,
                                  const Var& action) {
  if (has_interface<PPB_PDF>()) {
    get_interface<PPB_PDF>()->UserMetricsRecordAction(instance.pp_instance(),
                                                      action.pp_var());
  }
}

// static
void PDF::HasUnsupportedFeature(const InstanceHandle& instance) {
  if (has_interface<PPB_PDF>())
    get_interface<PPB_PDF>()->HasUnsupportedFeature(instance.pp_instance());
}

// static
void PDF::ShowAlertDialog(const InstanceHandle& instance, const char* message) {
  if (has_interface<PPB_PDF>())
    get_interface<PPB_PDF>()->ShowAlertDialog(instance.pp_instance(), message);
}

// static
bool PDF::ShowConfirmDialog(const InstanceHandle& instance,
                            const char* message) {
  if (has_interface<PPB_PDF>()) {
    return get_interface<PPB_PDF>()->ShowConfirmDialog(instance.pp_instance(),
                                                       message);
  }
  return false;
}

// static
pp::Var PDF::ShowPromptDialog(const InstanceHandle& instance,
                              const char* message,
                              const char* default_answer) {
  if (has_interface<PPB_PDF>()) {
    return pp::Var(PASS_REF,
                   get_interface<PPB_PDF>()->ShowPromptDialog(
                       instance.pp_instance(), message, default_answer));
  }
  return pp::Var();
}

// static
void PDF::SaveAs(const InstanceHandle& instance) {
  if (has_interface<PPB_PDF>())
    get_interface<PPB_PDF>()->SaveAs(instance.pp_instance());
}

// static
void PDF::Print(const InstanceHandle& instance) {
  if (has_interface<PPB_PDF>())
    get_interface<PPB_PDF>()->Print(instance.pp_instance());
}

// static
bool PDF::IsFeatureEnabled(const InstanceHandle& instance,
                           PP_PDFFeature feature) {
  if (has_interface<PPB_PDF>())
    return PP_ToBool(get_interface<PPB_PDF>()->IsFeatureEnabled(
        instance.pp_instance(), feature));
  return false;
}

// static
void PDF::SetSelectedText(const InstanceHandle& instance,
                          const char* selected_text) {
  if (has_interface<PPB_PDF>()) {
    get_interface<PPB_PDF>()->SetSelectedText(instance.pp_instance(),
                                              selected_text);
  }
}

// static
void PDF::SetLinkUnderCursor(const InstanceHandle& instance, const char* url) {
  if (has_interface<PPB_PDF>())
    get_interface<PPB_PDF>()->SetLinkUnderCursor(instance.pp_instance(), url);
}

// static
void PDF::GetV8ExternalSnapshotData(const InstanceHandle& instance,
                                    const char** snapshot_data_out,
                                    int* snapshot_size_out) {
  if (has_interface<PPB_PDF>()) {
    get_interface<PPB_PDF>()->GetV8ExternalSnapshotData(
        instance.pp_instance(), snapshot_data_out, snapshot_size_out);
    return;
  }
  *snapshot_data_out = NULL;
  *snapshot_size_out = 0;
}

// static
void PDF::SetAccessibilityViewportInfo(
    const InstanceHandle& instance,
    const PP_PrivateAccessibilityViewportInfo* viewport_info) {
  if (has_interface<PPB_PDF>()) {
    get_interface<PPB_PDF>()->SetAccessibilityViewportInfo(
        instance.pp_instance(), viewport_info);
  }
}

// static
void PDF::SetAccessibilityDocInfo(
    const InstanceHandle& instance,
    const PP_PrivateAccessibilityDocInfo* doc_info) {
  if (has_interface<PPB_PDF>()) {
    get_interface<PPB_PDF>()->SetAccessibilityDocInfo(instance.pp_instance(),
                                                      doc_info);
  }
}

// static
void PDF::SetAccessibilityPageInfo(
    const InstanceHandle& instance,
    const PP_PrivateAccessibilityPageInfo* page_info,
    const std::vector<PrivateAccessibilityTextRunInfo>& text_runs,
    const std::vector<PP_PrivateAccessibilityCharInfo>& chars,
    const PrivateAccessibilityPageObjects& page_objects) {
  if (has_interface<PPB_PDF>()) {
    std::vector<PP_PrivateAccessibilityTextRunInfo> text_run_info(
        text_runs.size());
    for (size_t i = 0; i < text_runs.size(); ++i)
      ConvertPrivateAccessibilityTextRunInfo(text_runs[i], &text_run_info[i]);

    const std::vector<PrivateAccessibilityLinkInfo>& links = page_objects.links;
    std::vector<PP_PrivateAccessibilityLinkInfo> link_info(links.size());
    for (size_t i = 0; i < links.size(); ++i)
      ConvertPrivateAccessibilityLinkInfo(links[i], &link_info[i]);

    const std::vector<PrivateAccessibilityImageInfo>& images =
        page_objects.images;
    std::vector<PP_PrivateAccessibilityImageInfo> image_info(images.size());
    for (size_t i = 0; i < images.size(); ++i)
      ConvertPrivateAccessibilityImageInfo(images[i], &image_info[i]);

    PP_PrivateAccessibilityPageObjects pp_page_objects;
    pp_page_objects.links = link_info.data();
    pp_page_objects.link_count = link_info.size();
    pp_page_objects.images = image_info.data();
    pp_page_objects.image_count = image_info.size();

    get_interface<PPB_PDF>()->SetAccessibilityPageInfo(
        instance.pp_instance(), page_info, text_run_info.data(), chars.data(),
        &pp_page_objects);
  }
}

// static
void PDF::SetCrashData(const InstanceHandle& instance,
                       const char* pdf_url,
                       const char* top_level_url) {
  if (has_interface<PPB_PDF>()) {
    get_interface<PPB_PDF>()->SetCrashData(instance.pp_instance(), pdf_url,
                                           top_level_url);
  }
}

// static
void PDF::SelectionChanged(const InstanceHandle& instance,
                           const PP_FloatPoint& left,
                           int32_t left_height,
                           const PP_FloatPoint& right,
                           int32_t right_height) {
  if (has_interface<PPB_PDF>()) {
    get_interface<PPB_PDF>()->SelectionChanged(
        instance.pp_instance(), &left, left_height, &right, right_height);
  }
}

// static
void PDF::SetPluginCanSave(const InstanceHandle& instance, bool can_save) {
  if (has_interface<PPB_PDF>()) {
    get_interface<PPB_PDF>()->SetPluginCanSave(instance.pp_instance(),
                                               can_save);
  }
}

}  // namespace pp
