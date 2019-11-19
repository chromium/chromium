# Do NOT add chrome to the list below. We shouldn't be including files
# from src/chrome in src/content.
include_rules = [
  # The subdirectories in content/ will manually allow their own include
  # directories in content/ so we disallow all of them.
  "-content",
  "+content/app/resources/grit/content_resources.h",
  "+content/app/strings/grit",  # For generated headers
  "+content/common",
  "+content/grit",
  "+content/public/common",
  "+content/public/test",
  "+content/test",
  "+blink/public/resources/grit",
  "+cc",

  "-components",
  # Content can depend on components that are:
  #   1) related to the implementation of the web platform, or,
  #   2) shared code between third_party/blink and content
  # It should not depend on chrome features or implementation details, i.e. the
  # original components/ directories which was code split out from chrome/ to be
  # shared with iOS. This includes, but isn't limited to, browser features such
  # as autofill or extensions, and chrome implementation details such as
  # settings, packaging details, installation or crash reporting.

  "+components/services/filesystem",
  "+components/services/font/public",

  "+crypto",
  "+grit/blink_resources.h",

  "+dbus",
  "+gpu",
  "+media",
  "+mojo/core/embedder",
  "+mojo/public",
  "+net",
  "+ppapi",
  "+printing",
  "+sandbox",
  "+services/proxy_resolver/public/mojom",
  "+services/service_manager/embedder",
  "+services/service_manager/sandbox",
  "+services/service_manager/zygote",
  "+skia",

  # In general, content/ should not rely on google_apis, since URLs
  # and access tokens should usually be provided by the
  # embedder.
  #
  # There are a couple of specific parts of content that are excepted
  # from this rule, e.g. content/browser/speech/DEPS. These are cases of
  # implementations that are strongly tied to Google servers, i.e. we
  # don't expect alternate implementations to be provided by the
  # embedder.
  "-google_apis",

  # Don't allow inclusion of these other libs we shouldn't be calling directly.
  "-v8",
  "-tools",

  # Allow inclusion of third-party code:
  "+third_party/angle",
  "+third_party/boringssl/src/include",
  "+third_party/flac",
  "+third_party/mozilla",
  "+third_party/ocmock",
  "+third_party/re2",
  "+third_party/skia",
  "+third_party/sqlite",
  "+third_party/khronos",
  "+third_party/webrtc",
  "+third_party/webrtc_overrides",
  "+third_party/zlib/google",
  "+third_party/blink/public",

  "+ui/accelerated_widget_mac",
  "+ui/accessibility",
  "+ui/android",
  # Aura is analogous to Win32 or a Gtk, so it is allowed.
  "+ui/aura",
  "+ui/base",
  "+ui/compositor",
  "+ui/display",
  "+ui/events",
  "+ui/gfx",
  "+ui/gl",
  "+ui/latency",
  "+ui/native_theme",
  "+ui/resources/grit/ui_resources.h",
  "+ui/resources/grit/webui_resources.h",
  "+ui/resources/grit/webui_resources_map.h",
  "+ui/shell_dialogs",
  "+ui/snapshot",
  "+ui/strings/grit/ui_strings.h",
  "+ui/surface",
  "+ui/touch_selection",
  "+ui/wm",
  # Content knows about grd files, but the specifics of how to get a resource
  # given its id is left to the embedder.
  "-ui/base/l10n",
  "-ui/base/resource",
  # These files aren't related to grd, so they're fine.
  "+ui/base/l10n/l10n_util_android.h",
  "+ui/base/l10n/l10n_util_win.h",

  # Content shouldn't depend on views. While we technically don't need this
  # line, since the top level DEPS doesn't allow it, we add it to make this
  # explicit.
  "-ui/views",

  "+storage/browser",
  "+storage/common",

  # For generated JNI includes.
  "+content/public/android/content_jni_headers",
  "+content/public/android/jar_jni",
]

specific_include_rules = {
  ".*_browsertest[a-z_]*\.(cc|h|mm)": [
    # content -> content/shell dependency is disallowed, except browser tests.
    "+content/shell/browser",
    "+content/shell/common",
  ],
}
