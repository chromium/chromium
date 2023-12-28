This directory contains shared iOS lookalike code.

End-to-end tests are located in ios/chrome/browser/web/model. The
ShouldAllowResponse tab helper code needs to be unit tested here in components,
since lookalike_url_egtest.mm uses a custom policy decider that overrides that
method.
