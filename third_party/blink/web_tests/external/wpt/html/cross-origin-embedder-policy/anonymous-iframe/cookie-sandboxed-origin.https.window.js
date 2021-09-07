// META: script=/common/get-host-info.sub.js
// META: script=/common/utils.js
// META: script=../credentialless/resources/common.js
// META: script=../credentialless/resources/dispatcher.js
// META: script=./resources/common.js

// Baseline test showing in sandboxed origin iframe whether requests and
// navigation requests use credentials.

const origin = {
  same_origin: get_host_info().HTTPS_ORIGIN,
  cross_origin: get_host_info().HTTPS_REMOTE_ORIGIN,
};

const cookie = {
  key: "cookie-sandboxed-origin",
  same_origin: "same_origin",
  cross_origin: "cross_origin",
};

promise_test_parallel(async test => {
  await Promise.all([
    setCookie(origin.same_origin, cookie.key, cookie.same_origin +
      cookie_same_site_none),
    setCookie(origin.cross_origin, cookie.key, cookie.cross_origin +
      cookie_same_site_none)
  ]);

  // Navigation requests in sandboxed iframe use credentials.
  for (const iframe_origin of ["same_origin", "cross_origin"]) {
    promise_test_parallel(async test => {
      const cookie_value =
        await cookieFromNavigation(cookie.key,
                                   origin[iframe_origin],
                                   {sandbox: "allow-scripts"});
      assert_equals(cookie_value, cookie[iframe_origin]);
    }, `Sandboxed ${iframe_origin} iframe is loaded with credentials`);
  }

  const iframe = {
    same_origin: newIframe(origin.same_origin, {sandbox: "allow-scripts"}),
    cross_origin: newIframe(origin.cross_origin, {sandbox: "allow-scripts"}),
  };

  // Subresource requests from sandboxed documents use credentials.
  for (const iframe_origin of ["same_origin", "cross_origin"]) {
    for (const resource_origin of ["same_origin", "cross_origin"]) {
      for (const element of ["iframe", "img"]) {
        promise_test_parallel(async test => {
          const cookie_value = await cookieFromRequest(cookie.key,
                                                       iframe[iframe_origin],
                                                       origin[resource_origin],
                                                       element);
          assert_equals(cookie_value, cookie[resource_origin]);
        }, `${iframe_origin} sandboxed iframe sends ${resource_origin} ` +
          `credentials for <${element}> request"`);
      }
    }
  }

}, "Setup")
