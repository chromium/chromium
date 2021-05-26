// This script is loaded in HTTP and HTTPS contexts to validate
// PerformanceResourceTiming entries' attributes when reusing connections.

// Make the first request before calling 'attribute_test' so that only the
// second request's PerformanceResourceTiming entry will be interrogated.
// We don't check the first request's PerformanceResourceTiming entry because
// that's not what this test is trying to validate.
//
// Note: to ensure that we reuse the connection to fetch multiple resources,
// we use the same XMLHttpRequest object for each request. Although it doesn't
// seem to be specified, each browser tested by WPT will reuse the underlying
// TCP connection with this approach. Pre-establishing the XHR's connection
// helps us to test connection reuse also in browsers that may key their
// connections on the related request's credentials mode.
const client = new XMLHttpRequest();
const identifier = Math.random();
const path = `resources/fake_responses.py?tag=${identifier}`;
client.open("GET", path, false);
client.send();

attribute_test(
  async () => {
    client.open("GET", path + "&same_resource=false", false);
    client.send();

    // We expect to get a 200 Ok response because we've requested a different
    // resource than previous requests.
    if (client.status != 200) {
      throw new Error(`Got something other than a 200 response. ` +
                      `client.status: ${client.status}`);
    }
  }, path, entry => {
    invariants.assert_connection_reused(entry);

    // The entry must also follow the specification for any entry corresponding
    // to a 'typical' 200 Ok response.
    if (self.location.protocol == 'https:') {
      invariants.assert_tao_pass_no_redirect_https(entry);
    } else {
      invariants.assert_tao_pass_no_redirect_http(entry);
    }
  },
  "PerformanceResrouceTiming entries need to conform to the spec when a " +
  "distinct resource is fetched over a persistent connection");

attribute_test(
  async () => {
    client.open("GET", path, false);
    client.setRequestHeader("If-None-Match", identifier);
    client.send();

    // We expect to get a 304 Not Modified response because we've used a
    // matching 'identifier' for the If-None-Match header.
    if (client.status != 304) {
      throw new Error(`Got something other than a 304 response. ` +
                      `client.status: ${client.status}`);
    }
  }, path, entry => {
    invariants.assert_connection_reused(entry);

    // The entry must also follow the specification for any entry corresponding
    // to a 'typical' 304 Not Modified response.
    if (self.location.protocol == 'https:') {
      invariants.assert_tao_pass_304_not_modified_https(entry);
    } else {
      invariants.assert_tao_pass_304_not_modified_http(entry);
    }
  },
  "PerformanceResrouceTiming entries need to conform to the spec when the " +
  "resource is cache-revalidated over a persistent connection");
