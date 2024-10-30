#include "content/browser/web_exposed_isolation_info.h"

#include <optional>

namespace content {

int InvalidOperators() {
  WebExposedIsolationInfo value = WebExposedIsolationInfo::CreateNonIsolated();
  std::optional<WebExposedIsolationInfo> optional_value =
      WebExposedIsolationInfo::CreateNonIsolated();

  value == optional_value;  // expected-error {{overload resolution selected deleted operator '=='}}
  optional_value == value;  // expected-error {{overload resolution selected deleted operator '=='}}
  optional_value == optional_value;  // expected-error {{overload resolution selected deleted operator '=='}}

  value != optional_value;  // expected-error {{overload resolution selected deleted operator '!='}}
  optional_value != value;  // expected-error {{overload resolution selected deleted operator '!='}}
  optional_value != optional_value;  // expected-error {{overload resolution selected deleted operator '!='}}
}

}  // namespace content
