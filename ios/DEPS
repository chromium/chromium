include_rules = [
  # The subdirectories in ios/ will manually allow their own include
  # directories in ios/ so we disallow all of them.
  "-ios",

  # For unit tests.
  "+ios/testing",
  "+components/test/ios",
  "+third_party/ocmock",
]

specific_include_rules = {
  ".*test\\.mm": [
    "+services/network/public/cpp",
    "+services/network/test",
  ],
}
