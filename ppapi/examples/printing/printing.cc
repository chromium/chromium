// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stddef.h>
#include <stdint.h>
#include <string.h>

#include "ppapi/cpp/dev/buffer_dev.h"
#include "ppapi/cpp/dev/printing_dev.h"
#include "ppapi/cpp/image_data.h"
#include "ppapi/cpp/instance.h"
#include "ppapi/cpp/module.h"
#include "ppapi/cpp/rect.h"
#include "ppapi/utility/completion_callback_factory.h"

namespace {

const char pdf_data[] = "%PDF-1.4\r"
"%    \r"
"1 0 obj<</Type/FontDescriptor/FontBBox[-50 -207 1447 1000]/FontName/Verdana/Flags 32/StemV 92/CapHeight 734/XHeight 546/Ascent 1005/Descent -209/ItalicAngle 0/FontFamily(Verdana)/FontStretch/Normal/FontWeight 400>>\r"
"2 0 obj<</Type/Font/Subtype/TrueType/Encoding/UniCNS-UTF16-H/BaseFont/Verdana/Name/Verdana/FontDescriptor 1 0 R/FirstChar 72/LastChar 114/Widths[ 750 420 454 692 556 843 748 787 602 787 695 684 616 731 684 989 685 614 685 454 454 454 818 635 635 600 623 521 623 595 351 623 633 274 343 591 274 972 633 607 623 623 426]>>\r"
"4 0 obj<</Type/Page/Parent 3 0 R/MediaBox[0 0 612 792]/Contents 5 0 R/Resources<</ProcSet[/PDF/Text/ColorC]/Font<</N2 2 0 R>> >> >>endobj\r"
"5 0 obj<</Length 70>>stream\r"
"BT/N2 24 Tf 100 692 Td(Hello)Tj ET\r"
"BT/N2 24 Tf 200 692 Td(World)Tj ET\r"
"\r"
"endstream\r"
"endobj\r"
"3 0 obj<</Type/Pages/Kids[ 4 0 R]/Count 1>>endobj\r"
"6 0 obj<</Type/Catalog/Pages 3 0 R>>endobj\r"
"xref\r"
"0 7\r"
"0000000000 65535 f \r"
"0000000015 00000 n \r"
"0000000230 00000 n \r"
"0000000805 00000 n \r"
"0000000551 00000 n \r"
"0000000689 00000 n \r"
"0000000855 00000 n \r"
"trailer<</Size 7/Root 6 0 R>>\r"
"startxref\r"
"898\r"
"%%EOF";

}  // namespace

class MyInstance : public pp::Instance, public pp::Printing_Dev {
 public:
  explicit MyInstance(PP_Instance instance)
      : pp::Instance(instance),
        pp::Printing_Dev(this) {
  }
  virtual ~MyInstance() {}

  virtual bool Init(uint32_t argc, const char* argn[], const char* argv[]) {
    return true;
  }

  virtual uint32_t QuerySupportedPrintOutputFormats() {
    return PP_PRINTOUTPUTFORMAT_PDF;
  }

  virtual int32_t PrintBegin(const PP_PrintSettings_Dev& print_settings) {
    return 1;
  }

  virtual pp::Resource PrintPages(
      const PP_PrintPageNumberRange_Dev* page_ranges,
      uint32_t page_range_count) {
    size_t pdf_len = strlen(pdf_data);
    pp::Buffer_Dev buffer(this, static_cast<uint32_t>(pdf_len));

    memcpy(buffer.data(), pdf_data, pdf_len);
    return buffer;
  }

  virtual void PrintEnd() {
  }

  virtual bool IsPrintScalingDisabled() {
    return true;
  }
};

class MyModule : public pp::Module {
 public:
  MyModule() : pp::Module() {}
  virtual ~MyModule() {}

  // Override CreateInstance to create your customized Instance object.
  virtual pp::Instance* CreateInstance(PP_Instance instance) {
    return new MyInstance(instance);
  }
};

namespace pp {

// Factory function for your specialization of the Module object.
Module* CreateModule() {
  return new MyModule();
}

}  // namespace pp

