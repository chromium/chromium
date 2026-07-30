Tests in this directory have been moved from
web_tests/http/tests/inspector-protocol/network, where they were originally
covering the Network domain support for interception. While network interception
implementation was shared between both Network and Fetch domains, the APIs have
a slightly different shape and occasionally differ in exposed features. These
tests have been rewritten to provide coverage for the Fetch domain by
https://crrev.com/c/8157363, but no effort has been made to otherwise modernize
the tests or remove duplicates. As time permits, these tests should ideally
undergo additional review and those deemed still relevant should be promoted to
the parent directory.
