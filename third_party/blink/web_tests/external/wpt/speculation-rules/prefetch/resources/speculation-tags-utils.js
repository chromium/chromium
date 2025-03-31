// Utilities for speculation tags tests.

function testRulesetTag(tag, expectedTag, description) {
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
  }, "Sec-Speculation-Tags: " + description);
}
