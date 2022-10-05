async function assertSimplestScriptRuns(remoteContextWrapper) {
  assert_equals(
      await remoteContextWrapper.executeScript(() => {
        return 1;
      }),
      1, 'simplest script runs');
}

async function assertFunctionRuns(
    remoteContextWrapper, functionToRun, expectedReturn) {
  assert_equals(
      await remoteContextWrapper.executeScript(functionToRun), expectedReturn,
      'function runs');
}

async function assertOriginIsAsExpected(remoteContextWrapper, expectedOrigin) {
  assert_equals(
      await remoteContextWrapper.executeScript(() => {
        return location.origin;
      }),
      expectedOrigin, 'verify origin');
}

async function assertWindowNameEquals(remoteContextWrapper, expectedName) {
  assert_equals(
      await remoteContextWrapper.executeScript(() => {
        return window.name;
      }),
      expectedName, 'verify name');
}

async function assertWindowHasOpenerEquals(remoteContextWrapper, hasParent) {
  assert_equals(
      await remoteContextWrapper.executeScript(() => {
        return !!window.opener;
      }),
      hasParent, 'verify opener');
}

async function assertHeaderIsAsExpected(
    remoteContextWrapper, headerName, headerValue) {
  assert_equals(
      headerValue,
      await remoteContextWrapper.executeScript(async (headerName) => {
        const res = await fetch(location);
        return res.headers.get(headerName);
      }, [headerName]), 'header is set');
}

async function  assertNotRestoredReasonsEquals(
  remoteContextHelper, blocked, url, src, id, name, reasons, children) {
  let result = await remoteContextHelper.executeScript(() => {
    return performance.getEntriesByType('navigation')[0].notRestoredReasons;
  });
  assertReasonsStructEquals(result, blocked, url, src, id, name, reasons, children);
}

function assertReasonsStructEquals(result, blocked, url, src, id, name, reasons, children) {
  assert_equals(result.blocked, blocked);
  assert_equals(result.url, url);
  assert_equals(result.src, src);
  assert_equals(result.id, id);
  assert_equals(result.name, name);
  // Reasons should match.
  assert_equals(result.reasons.length, reasons.length);
  reasons.sort();
  result.reasons.sort();
  for (let i=0; i<reasons.length; i++) {
    assert_equals(result.reasons[i], reasons[i]);
  }
  // Children should match.
  assert_equals(result.children.length, children.length);
  children.sort();
  result.children.sort();
  for (let j=0; j<children.length; j++) {
    assertReasonsStructEquals(result.children[0],
      children[0].blocked,
      children[0].url,
      children[0].src,
      children[0].id,
      children[0].name,
      children[0].reasons,
      children[0].children);
  }
}