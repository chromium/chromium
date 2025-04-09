// Utilities for speculation tags tests.

{
  // Retrieves a tag level variant from URLSearchParams of the current window and
  // returns it. Throw an Error if it doesn't have the valid tag level param.
  function getTagLevel() {
    const params = new URLSearchParams(window.location.search);
    const level = params.get('tag-level');
    if (level === null)
      throw new Error('window.location does not have a tag-level param');
    if (level !== 'ruleset' && level !== 'rule')
      throw new Error('window.location does not have a valid tag-level param');
    return level;
  }

  // Retrieves a preloading type variant from URLSearchParams of the current
  // window and returns it. Throw an Error if it doesn't have the valid
  // preloading type param.
  function getPreloadingType() {
    const params = new URLSearchParams(window.location.search);
    const level = params.get('type');
    if (level === null)
      throw new Error('window.location does not have a preloading type param');
    if (level !== 'prefetch' && level !== 'prerender')
      throw new Error('window.location does not have a valid preloading type param');
    return level;
  }

  function testPrefetchRulesetTag(tag, expectedTag, description) {
    promise_test(async t => {
        const agent = await spawnWindow(t);
        const nextUrl = agent.getExecutorURL({ page: 2 });
        await agent.forceSpeculationRules({
            tag,
            prefetch: [{source: "list", urls: [nextUrl]}]
        });
        await agent.navigate(nextUrl);

        const headers = await agent.getRequestHeaders();
        assert_prefetched(headers, "must be prefetched");
        assert_equals(headers.sec_speculation_tags, expectedTag, "Sec-Speculation-Tags");
    }, "Sec-Speculation-Tags [ruleset-based]: " + description);
  }

  function testPrefetchInvalidRulesetTag(tag, description) {
    testPrefetchRulesetTag(tag, 'null', description);
  }

  function testPrefetchRuleTag(tag, expectedTag, description) {
    promise_test(async t => {
        const agent = await spawnWindow(t);
        const nextUrl = agent.getExecutorURL({ page: 2 });
        await agent.forceSpeculationRules({
            prefetch: [{tag, source: "list", urls: [nextUrl]}]
        });
        await agent.navigate(nextUrl);

        const headers = await agent.getRequestHeaders();
        assert_prefetched(headers, "must be prefetched");
        assert_equals(headers.sec_speculation_tags, expectedTag, "Sec-Speculation-Tags");
    }, "Sec-Speculation-Tags [rule-based]: " + description);
  }

  function testPrefetchInvalidRuleTag(tag, description) {
    promise_test(async t => {
        const agent = await spawnWindow(t);
        const nextUrl = agent.getExecutorURL({ page: 2 });
        await agent.forceSpeculationRules({
            prefetch: [{tag, source: "list", urls: [nextUrl]}]
        });
        await agent.navigate(nextUrl);

        const headers = await agent.getRequestHeaders();
        assert_not_prefetched(headers, "must not be prefetched");
        assert_equals(headers.sec_speculation_tags, "", "Sec-Speculation-Tags");
    }, "Sec-Speculation-Tags [rule-based]: " + description);
  }

  function testPrerenderRulesetTag(tag, expectedTag, description) {
    promise_test(async t => {
        const rcHelper = new RemoteContextHelper();
        const referrerRC = await rcHelper.addWindow(undefined, { features: 'noopener' });

        const extraConfig = {};
        const prerenderedRC = await referrerRC.helper.createContext({
            executorCreator(url) {
              return referrerRC.executeScript((tag, url) => {
                  const script = document.createElement("script");
                  script.type = "speculationrules";
                  script.textContent = JSON.stringify({
                      tag,
                      prerender: [
                        {
                          source: "list",
                          urls: [url]
                        }
                      ]
                  });
                  document.head.append(script);
              }, [tag, url]);
            }, extraConfig
        });

        // Check the prerender request headers embedded in the prerendered page.
        // Don't need to activate the page.
        const headers = await prerenderedRC.getRequestHeaders();
        assert_equals(headers.get("sec-purpose"), "prefetch;prerender");
        assert_equals(headers.get("sec-speculation-tags"), expectedTag);
    }, "Sec-Speculation-Tags [ruleset-based]: " + description);
  }

  function testPrerenderInvalidRulesetTag(tag, description) {
    testPrerenderRulesetTag(tag, 'null', description);
  }

  function testPrerenderRuleTag(tag, expectedTag, description) {
    promise_test(async t => {
        const rcHelper = new RemoteContextHelper();
        const referrerRC = await rcHelper.addWindow(undefined, { features: 'noopener' });

        const extraConfig = {};
        const prerenderedRC = await referrerRC.helper.createContext({
            executorCreator(url) {
              return referrerRC.executeScript((tag, url) => {
                  const script = document.createElement("script");
                  script.type = "speculationrules";
                  script.textContent = JSON.stringify({
                      prerender: [
                        {
                          tag,
                          source: "list",
                          urls: [url]
                        }
                      ]
                  });
                  document.head.append(script);
              }, [tag, url]);
            }, extraConfig
        });

        // Check the prerender request headers embedded in the prerendered page.
        // Don't need to activate the page.
        const headers = await prerenderedRC.getRequestHeaders();
        assert_equals(headers.get("sec-purpose"), "prefetch;prerender");
        assert_equals(headers.get("sec-speculation-tags"), expectedTag);
    }, "Sec-Speculation-Tags [rule-based]: " + description);
  }

  function testPrerenderInvalidRuleTag(tag, description) {
    promise_test(async t => {
        const rcHelper = new RemoteContextHelper();
        const referrerRC = await rcHelper.addWindow(undefined, { features: 'noopener' });

        const extraConfig = {};
        const prerenderedRC = await referrerRC.helper.createContext({
            executorCreator(url) {
              return referrerRC.executeScript((tag, url) => {
                  const script = document.createElement("script");
                  script.type = "speculationrules";
                  script.textContent = JSON.stringify({
                      prerender: [
                        {
                          tag,
                          source: "list",
                          urls: [url]
                        }
                      ]
                  });
                  document.head.append(script);
              }, [tag, url]);
            }, extraConfig
        });

        // Prerender should fail when an invalid tag is specified, and this
        // navigation should fall back to network.
        await referrerRC.navigateTo(prerenderedRC.url);

        // Confirm that the page was loaded from network, not activated, by
        // checking the prerender request headers.
        const headers = await prerenderedRC.getRequestHeaders();
        assert_false(headers.has("sec-purpose"));
        assert_false(headers.has("sec-speculation-tags"));
    }, "Sec-Speculation-Tags [rule-based]: " + description);
  }

  // Runs the test function for valid tag cases based on the tag level and the
  // preloading type.
  globalThis.testTag = (tag, expectedTag, description) => {
    if (getTagLevel() === 'ruleset') {
      if (getPreloadingType() === 'prefetch') {
        testPrefetchRulesetTag(tag, expectedTag, description);
      } else {
        testPrerenderRulesetTag(tag, expectedTag, description);
      }
    } else {
      if (getPreloadingType() === ' prefetch') {
        testPrefetchRuleTag(tag, expectedTag, description);
      } else {
        testPrerenderRuleTag(tag, expectedTag, description);
      }
    }
  };

  // Runs the test function for invalid tag cases based on the tag level and the
  // preloading type.
  globalThis.testInvalidTag = (tag, description) => {
    if (getTagLevel() === 'ruleset') {
      if (getPreloadingType() === 'prefetch') {
        testPrefetchInvalidRulesetTag(tag, description);
      } else {
        testPrerenderInvalidRulesetTag(tag, description);
      }
    } else {
      if (getPreloadingType() === 'prefetch') {
        testPrefetchInvalidRuleTag(tag, description);
      } else {
        testPrerenderInvalidRuleTag(tag, description);
      }
    }
  };
}
