// META: script=/common/get-host-info.sub.js
// META: script=/common/utils.js
// META: script=../credentialless/resources/common.js
// META: script=../credentialless/resources/dispatcher.js
// META: timeout=long

const same_origin = get_host_info().HTTPS_ORIGIN;
const cross_origin = get_host_info().HTTPS_REMOTE_ORIGIN;

const CROSS_ORIGIN_ISOLATED = "true";
const NOT_CROSS_ORIGIN_ISOLATED = "false";
const BLOCKED = "blocked";

// Vary the headers of a parent and its anonymous iframe. Determine in which
// cases the anonymous iframe gets the cross-origin-isolated capability.
const crossOriginIsolatedTest = (description, params) => {
  const default_params = {
    parent_origin: same_origin,
    parent_headers: '',
    parent_allow: '',
    child_origin: same_origin,
    child_headers: '',
    child_state: NOT_CROSS_ORIGIN_ISOLATED,
  };
  params = {...default_params, ...params};

  promise_test_parallel(async test => {
    // Create the parent.
    const parent_token = token();
    const parent_url = params.parent_origin + executor_path +
        params.parent_headers + `&uuid=${parent_token}`;
    const parent = window.open(parent_url)
    add_completion_callback(() => parent.close());

    // Create its anonymous iframe.
    const child_token = token();
    const child_url = params.child_origin + executor_path +
        params.child_headers + `&uuid=${child_token}`;
    send(parent_token, `
      const iframe = document.createElement("iframe");
      iframe.src = "${child_url}";
      iframe.anonymous = true;
      iframe.allow="${params.parent_allow}";
      document.body.appendChild(iframe);
    `);

    // Check child's cross-origin isolation state.
    const this_token = token();
    send(child_token, `
      send("${this_token}", window.crossOriginIsolated);
    `);

    test.step_timeout(() => {
      send(this_token, 'blocked');
    }, 3000);

    assert_equals(await receive(this_token), params.child_state);
  }, description);
};

crossOriginIsolatedTest("Basic", {
  child_state: NOT_CROSS_ORIGIN_ISOLATED,
});

crossOriginIsolatedTest("Basic + child cross_origin", {
  child_origin: cross_origin,
  child_state: NOT_CROSS_ORIGIN_ISOLATED,
});

crossOriginIsolatedTest("Parent coep_require-corp", {
  parent_headers: coep_require_corp,
  child_state: NOT_CROSS_ORIGIN_ISOLATED,
});

crossOriginIsolatedTest("Parent coep_require-corp + cross_origin", {
  parent_headers: coep_require_corp,
  child_origin: cross_origin,
  child_state: NOT_CROSS_ORIGIN_ISOLATED,
});

crossOriginIsolatedTest("Parent COI", {
  parent_headers: coop_same_origin + coep_require_corp,
  child_state: CROSS_ORIGIN_ISOLATED,
});

crossOriginIsolatedTest("Parent COI + child cross-origin", {
  parent_headers: coop_same_origin + coep_require_corp,
  child_origin: cross_origin,
  child_state: NOT_CROSS_ORIGIN_ISOLATED,
});

crossOriginIsolatedTest("Parent COI + child cross-origin COEP/CORP", {
  parent_headers: coop_same_origin + coep_require_corp,
  child_headers: coep_require_corp + corp_cross_origin,
  child_origin: cross_origin,
  child_state: NOT_CROSS_ORIGIN_ISOLATED,
});

crossOriginIsolatedTest('Parent COI allow + child cross_origin', {
  parent_headers: coop_same_origin + coep_require_corp,
  parent_allow: 'cross-origin-isolated',
  child_origin: cross_origin,
  child_state: CROSS_ORIGIN_ISOLATED,
});

crossOriginIsolatedTest('Parent COI allow + child cross-origin COEP/CORP', {
  parent_headers: coop_same_origin + coep_require_corp,
  parent_allow: 'cross-origin-isolated',
  child_headers: coep_require_corp + corp_cross_origin,
  child_origin: cross_origin,
  child_state: CROSS_ORIGIN_ISOLATED,
});
