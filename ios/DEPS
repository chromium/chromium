include_rules = [
  # The subdirectories in ios/ will manually allow their own include
  # directories in ios/ so we disallow all of them.
  "-ios",

  # For unit tests.
  "+ios/testing",
  "+components/test/ios",
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
