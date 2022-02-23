// The test functions called in the navigation-counter test. They rely on
// artifacts defined in
// '/html/browsers/browsing-the-web/back-forward-cache/resources/helper.sub.js'
// which should be included before this file to use these functions.

function runNavigationCounterTest(params, description) {
  const defaultParams = {
    constants: {
      performanceMarkName: 'mark_navigation_counter',
      performanceMeasureName: 'measure_navigation_counter',
    },
    // This function is to make and obtain the navigation counter value for a
    // performance entries of mark and measure type. It is to be extended for
    // other types of performance entry in future.
    funcBeforeNavigation: (constants) => {
      window.performance.mark(constants.performanceMarkName);
      return window.performance
        .getEntriesByName(constants.performanceMarkName)[0]
        .navigationCount;
    },
    funcAfterBFCacheLoad: (expectedNavigationCount, constants) => {
      window.performance.mark(
        constants.performanceMarkName + expectedNavigationCount);
      window.performance.measure(
        constants.performanceMeasureName + expectedNavigationCount,
        constants.performanceMarkName,
        constants.performanceMarkName + expectedNavigationCount);
      return [
        window.performance
          .getEntriesByName(
            constants.performanceMarkName + expectedNavigationCount)[0]
          .navigationCount,
        window.performance
          .getEntriesByName(
            constants.performanceMeasureName + expectedNavigationCount)[0]
          .navigationCount
      ];
    },
  };
  params = { ...defaultParams, ...params };
  runBfcacheWithMultipleNavigationTest(params, description);
}

function runBfcacheWithMultipleNavigationTest(params, description) {
  const defaultParams = {
    openFunc: url => window.open(url, '_blank', 'noopener'),
    scripts: [],
    funcBeforeNavigation: () => { },
    targetOrigin: originCrossSite,
    navigationTimes: 1,
    funcAfterAssertion: () => { },
  }  // Apply defaults.
  params = { ...defaultParams, ...params };

  promise_test(async t => {
    const pageA = new RemoteContext(token());
    const pageB = new RemoteContext(token());

    const urlA = executorPath + pageA.context_id;
    const urlB = params.targetOrigin + executorPath + pageB.context_id;

    params.openFunc(urlA);

    await pageA.execute_script(waitForPageShow);

    // Assert navigation counter is 0 when the document is loaded first time.
    let navigationCount = await pageA.execute_script(
      params.funcBeforeNavigation, [params.constants])
    assert_implements_optional(
      navigationCount === 0, 'NavigationCount should be 0.');

    for (i = 0; i < params.navigationTimes; i++) {
      await navigateAndThenBack(pageA, pageB, urlB);

      let navigationCounts = await pageA.execute_script(
        params.funcAfterBFCacheLoad, [i + 1, params.constants]);
      assert_implements_optional(
        navigationCounts.every(t => t === (i + 1)),
        'NavigationCount should all be ' + (i + 1) + '.');
    }
  }, description);
}
