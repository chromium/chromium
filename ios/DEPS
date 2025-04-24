include_rules = [
  # The subdirectories in ios/ will manually allow their own include
  # directories in ios/ so we disallow all of them.
  "-ios",

  # To avoid ODR violation, direct import of these libraries is forbidden in
  # ios/DEPS and code should instead use import as if they were in a framework
  # (i.e. #import <OCHamcrest/OCHamcrest.h>).
  # TODO(crbug.com/40138188): Disallow direct import of eg2 headers.
  "-ios/third_party/ochamcrest",

  # For unit tests.
  "+ios/testing",
  "+third_party/ocmock",

  # To avoid includes from web::HttpServer since it is deprecated. Reference
  # to crbug.com/708307.
  "-ios/web/public/test/http_server/http_server.h",
]

specific_include_rules = {
  ".*test\.mm": [
    "+services/network/public/cpp",
    "+services/network/test",
  ],
}
