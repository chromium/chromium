async function assertNotRestoredReasonsEquals(
    remoteContextHelper, blocked, url, src, id, name, reasons, children) {
  let result = await remoteContextHelper.executeScript(() => {
    return performance.getEntriesByType('navigation')[0].notRestoredReasons;
  });
  assertReasonsStructEquals(
      result, blocked, url, src, id, name, reasons, children);
}

function assertReasonsStructEquals(
    result, blocked, url, src, id, name, reasons, children) {
  assert_equals(result.blocked, blocked);
  assert_equals(result.url, url);
  assert_equals(result.src, src);
  assert_equals(result.id, id);
  assert_equals(result.name, name);
  // Reasons should match.
  assert_equals(result.reasons.length, reasons.length);
  reasons.sort();
  result.reasons.sort();
  for (let i = 0; i < reasons.length; i++) {
    assert_equals(result.reasons[i], reasons[i]);
  }
  // Children should match.
  assert_equals(result.children.length, children.length);
  children.sort();
  result.children.sort();
  for (let j = 0; j < children.length; j++) {
    assertReasonsStructEquals(
        result.children[0], children[0].blocked, children[0].url,
        children[0].src, children[0].id, children[0].name, children[0].reasons,
        children[0].children);
  }
}

// Requires:
// - /websockets/constants.sub.js in the test file and pass the domainPort
// constant here.
async function useWebSocket(remoteContextHelper) {
  await remoteContextHelper.executeScript((domain) => {
    var webSocketInNotRestoredReasonsTests = new WebSocket(domain + '/echo');
  }, [SCHEME_DOMAIN_PORT]);
}

// Call |prepareForBFCache()| before navigating away from the page. This simply
// sets a variable in window.
async function prepareForBFCache(remoteContextHelper) {
  await remoteContextHelper.executeScript(() => {
    window.beforeBFCache = true;
  });
}

// Call |getBeforeCache| after navigating back to the page. This returns the
// value in window.
async function getBeforeBFCache(remoteContextHelper) {
  return await remoteContextHelper.executeScript(() => {
    return window.beforeBFCache;
  });
}

// If the value in window is set to true, this means that the page was reloaded,
// i.e., the page was restored from BFCache.
// Call |prepareForBFCache()| before navigating away to call this function.
async function assert_implements_bfcache(remoteContextHelper) {
  var beforeBFCache = await getBeforeBFCache(remoteContextHelper);
  assert_implements_optional(beforeBFCache == true, 'BFCache not supported.');
}

// If the value in window is undefined, this means that the page was reloaded,
// i.e., the page was not restored from BFCache.
// Call |prepareForBFCache()| before navigating away to call this function.
async function assert_not_bfcached(remoteContextHelper) {
  var beforeBFCache = await getBeforeBFCache(remoteContextHelper);
  assert_equals(beforeBFCache, undefined);
}
