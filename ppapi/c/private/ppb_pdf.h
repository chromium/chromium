// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PPAPI_C_PRIVATE_PPB_PDF_H_
#define PPAPI_C_PRIVATE_PPB_PDF_H_

#include <stdint.h>

#include "ppapi/c/pp_bool.h"
#include "ppapi/c/pp_instance.h"
#include "ppapi/c/pp_point.h"
#include "ppapi/c/pp_rect.h"
#include "ppapi/c/pp_resource.h"
#include "ppapi/c/pp_var.h"
#include "ppapi/c/private/pp_private_font_charset.h"

#define PPB_PDF_INTERFACE "PPB_PDF;1"

typedef enum {
  PP_PDFFEATURE_HIDPI = 0,
  PP_PDFFEATURE_PRINTING = 1
} PP_PDFFeature;

typedef enum {
  PP_CONTENT_RESTRICTION_COPY = 1 << 0,
  PP_CONTENT_RESTRICTION_CUT = 1 << 1,
  PP_CONTENT_RESTRICTION_PASTE = 1 << 2,
  PP_CONTENT_RESTRICTION_PRINT = 1 << 3,
  PP_CONTENT_RESTRICTION_SAVE = 1 << 4
} PP_ContentRestriction;

struct PP_PrivateFontFileDescription {
  const char* face;
  uint32_t weight;
  bool italic;
};

struct PP_PrivateFindResult {
  int start_index;
  int length;
};

struct PPB_PDF {
  // Returns a resource identifying a font file corresponding to the given font
  // request after applying the browser-specific fallback.
  //
  // Currently Linux-only.
  PP_Resource (*GetFontFileWithFallback)(
      PP_Instance instance,
      const struct PP_BrowserFont_Trusted_Description* description,
      PP_PrivateFontCharset charset);

  // Given a resource previously returned by GetFontFileWithFallback, returns
  // a pointer to the requested font table. Linux only.
  bool (*GetFontTableForPrivateFontFile)(PP_Resource font_file,
                                         uint32_t table,
                                         void* output,
                                         uint32_t* output_length);

  // Search the given string using ICU.  Use PPB_Core's MemFree on results when
  // done.
  void (*SearchString)(PP_Instance instance,
                       const unsigned short* string,
                       const unsigned short* term,
                       bool case_sensitive,
                       struct PP_PrivateFindResult** results,
                       uint32_t* count);

  // Since WebFrame doesn't know about PPAPI requests, it'll think the page has
  // finished loading even if there are outstanding requests by the plugin.
  // Take this out once WebFrame knows about requests by PPAPI plugins.
  void (*DidStartLoading)(PP_Instance instance);
  void (*DidStopLoading)(PP_Instance instance);

  // Sets content restriction for a full-page plugin (i.e. can't copy/print).
  // The value is a bitfield of PP_ContentRestriction enums.
  void (*SetContentRestriction)(PP_Instance instance, int restrictions);

  // Notifies the browser that the given action has been performed.
  void (*UserMetricsRecordAction)(PP_Instance instance, struct PP_Var action);

  // Notifies the browser that the PDF has an unsupported feature.
  void (*HasUnsupportedFeature)(PP_Instance instance);

  // Invoke SaveAs... dialog, similar to the right-click or wrench menu.
  void (*SaveAs)(PP_Instance instance);

  // Invoke Print dialog for plugin.
  void (*Print)(PP_Instance instance);

  PP_Bool(*IsFeatureEnabled)(PP_Instance instance, PP_PDFFeature feature);

  // Sets the selected text of the plugin.
  void(*SetSelectedText)(PP_Instance instance, const char* selected_text);

  // Sets the link currently under the cursor.
  void (*SetLinkUnderCursor)(PP_Instance instance, const char* url);

  // Gets pointers to the mmap'd V8 snapshot file and its size.
  // This is needed when loading V8's initial snapshot from an external file.
  void (*GetV8ExternalSnapshotData)(PP_Instance instance,
                                    const char** snapshot_data_out,
                                    int* snapshot_size_out);

  // Sends information about the PDF's URL and the embedder's URL.
  void (*SetCrashData)(PP_Instance instance,
                       const char* pdf_url,
                       const char* top_level_url);

  // Sets the current selection bounding edges.
  void (*SelectionChanged)(PP_Instance instance,
                           const struct PP_FloatPoint* left,
                           int32_t left_height,
                           const struct PP_FloatPoint* right,
                           int32_t right_height);

  // Sets whether the PDF viewer can handle save commands internally.
  void (*SetPluginCanSave)(PP_Instance instance, bool can_save);

  // Displays an alert dialog.
  void (*ShowAlertDialog)(PP_Instance instance, const char* message);

  // Displays a confirmation dialog. This method is synchronous.
  bool (*ShowConfirmDialog)(PP_Instance instance, const char* message);

  // Displays a prompt dialog. This method is synchronous.
  struct PP_Var (*ShowPromptDialog)(PP_Instance instance,
                                    const char* message,
                                    const char* default_answer);
};

#endif  // PPAPI_C_PRIVATE_PPB_PDF_H_
