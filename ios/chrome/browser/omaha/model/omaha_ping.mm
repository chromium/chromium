// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/omaha/model/omaha_ping.h"

#import <string>

#import "base/functional/bind.h"
#import "base/functional/callback.h"
#import "base/memory/raw_ref.h"
#import "base/strings/string_number_conversions.h"
#import "base/system/sys_info.h"
#import "ios/public/provider/chrome/browser/omaha/omaha_api.h"
#import "third_party/libxml/chromium/xml_writer.h"

namespace {

// Constants.
constexpr char kInstallEventType[] = "2";
constexpr char kUpdateEventType[] = "3";
constexpr char kSuccessEventResult[] = "1";
constexpr char kArchitecture[] = "arm64";
constexpr char kFirstInstallAge[] = "-1";

// Helper to write XML elements to an XmlWriter.
class XmlElement {
 public:
  XmlElement(XmlWriter& writer, const std::string& name) : writer_(writer) {
    writer_->StartElement(name);

    ios::provider::SetOmahaExtraAttributes(
        name, base::BindRepeating(base::IgnoreResult(&XmlElement::AddAttribute),
                                  base::Unretained(this)));
  }

  ~XmlElement() { writer_->EndElement(); }

  XmlElement AddElement(const std::string& name) {
    return XmlElement(*writer_, name);
  }

  XmlElement& AddAttribute(const std::string& name, const std::string& value) {
    writer_->AddAttribute(name, value);
    return *this;
  }

 private:
  const raw_ref<XmlWriter> writer_;
};

// Returns whether `installation_time` is valid.
bool IsInstallationTimeValid(base::Time installation_time) {
  // 2 is used because 0 is a magic value for Time, and 1 was the pre-M29 value
  // which was migrated to a specific date (crbug.com/270124).
  const int64_t kUnknownInstallDate = 2;

  return !installation_time.is_null() &&
         installation_time.ToTimeT() != kUnknownInstallDate;
}

// Returns the event type for a ping.
const char* GetEventType(bool is_first_install) {
  return is_first_install ? kInstallEventType : kUpdateEventType;
}

}  // namespace

std::string FormatOmahaPingEvent(OmahaPingEvent event, OmahaPingData data) {
  XmlWriter writer;
  writer.StartWriting();
  writer.StopIndenting();

  const bool is_first_install = event == OmahaPingEvent::kInstallEvent &&
                                !data.previous_version.IsValid();

  {
    // <request ...>
    XmlElement request(writer, "request");
    request.AddAttribute("protocol", "3.0");
    request.AddAttribute("updater", "iOS");
    request.AddAttribute("updaterversion", data.current_version.GetString());
    request.AddAttribute("updaterchannel", data.channel_name);
    request.AddAttribute("ismachine", "1");
    request.AddAttribute("requestid", data.request_id);
    request.AddAttribute("sessionid", data.session_id);
    request.AddAttribute("hardware_class", data.hardware_class);

    {
      // <os ... />
      request.AddElement("os")
          .AddAttribute("platform", "ios")
          .AddAttribute("version", data.os_version)
          .AddAttribute("arch", kArchitecture);
    }

    {
      // <app ...>
      XmlElement app = request.AddElement("app");
      if (event == OmahaPingEvent::kInstallEvent) {
        const std::string previous_version =
            is_first_install ? "" : data.previous_version.GetString();
        app.AddAttribute("version", previous_version);
        app.AddAttribute("nextversion", data.current_version.GetString());
      } else {
        app.AddAttribute("version", data.current_version.GetString());
        app.AddAttribute("nextversion", "");
      }
      app.AddAttribute("ap", data.channel_name);
      app.AddAttribute("lang", data.locale_lang);
      app.AddAttribute("client", "");

      if (is_first_install) {
        app.AddAttribute("installage", kFirstInstallAge);
      } else if (IsInstallationTimeValid(data.installation_time)) {
        const int days = (base::Time::Now() - data.installation_time).InDays();
        app.AddAttribute("installage", base::NumberToString(days));
      }

      if (event == OmahaPingEvent::kInstallEvent) {
        // <event ... />
        app.AddElement("event")
            .AddAttribute("eventtype", GetEventType(is_first_install))
            .AddAttribute("eventresult", kSuccessEventResult);
      } else {
        // <updatecheck/>
        app.AddElement("updatecheck");
      }

      {
        // <ping ... />
        const std::string date = base::NumberToString(data.last_server_date);
        app.AddElement("ping")
            .AddAttribute("active", "1")
            .AddAttribute("ad", date)
            .AddAttribute("rd", date);
      }

      // </app>
    }

    // </request>
  }

  writer.StopWriting();
  return writer.GetWrittenString();
}
