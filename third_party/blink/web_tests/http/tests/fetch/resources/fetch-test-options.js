function get_fetch_test_options() {
  const {
    HTTP_ORIGIN: BASE_HTTP_ORIGIN,
    HTTP_REMOTE_ORIGIN: OTHER_HTTP_ORIGIN,
    HTTPS_ORIGIN: BASE_HTTPS_ORIGIN,
    HTTPS_REMOTE_ORIGIN: OTHER_HTTPS_ORIGIN
  } = get_host_info();
  var BASE_ORIGIN = BASE_HTTP_ORIGIN;
  var OTHER_ORIGIN = OTHER_HTTP_ORIGIN;
  var TEST_OPTIONS = '';
  // TEST_OPTIONS is '', '-other-https', '-base-https', or
  // '-base-https-other-https'.

  if (location.href.indexOf('base-https') >= 0) {
    BASE_ORIGIN = BASE_HTTPS_ORIGIN;
    TEST_OPTIONS += '-base-https';
  }

  if (location.href.indexOf('other-https') >= 0) {
    OTHER_ORIGIN = OTHER_HTTPS_ORIGIN;
    TEST_OPTIONS += '-other-https';
  }

  return {
    BASE_HTTP_ORIGIN: BASE_HTTP_ORIGIN,
    OTHER_HTTP_ORIGIN: OTHER_HTTP_ORIGIN,
    BASE_HTTPS_ORIGIN: BASE_HTTPS_ORIGIN,
    OTHER_HTTPS_ORIGIN: OTHER_HTTPS_ORIGIN,
    BASE_ORIGIN: BASE_ORIGIN,
    OTHER_ORIGIN: OTHER_ORIGIN,
    TEST_OPTIONS: TEST_OPTIONS
  };
}
