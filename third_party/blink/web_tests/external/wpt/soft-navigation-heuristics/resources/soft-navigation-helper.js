var counter = 0;
var clicked;
const max_clicks = 50;
const url = "/foobar.html";
const test_soft_navigation = (add_content, button, push_state, clicks,
                              extra_validations, test_name, push_url = true) => {
  promise_test(async t => {
    const pre_click_lcp = await get_lcp_entries();
    setClickEvent(t, button, push_state, add_content, push_url);
    for (let i = 0; i < clicks; ++i) {
      clicked = false;
      click(button);
      await new Promise(resolve => {
        (new PerformanceObserver(() => resolve())).observe(
          {type: 'soft-navigation'});
        });
    }
    assert_equals(document.softNavigations, clicks,
      "Soft Navigations detected are the same as the number of clicks");
    await validate_soft_navigation_entry(clicks, extra_validations, push_url);

    await double_raf();

    validate_paint_entries("first-contentful-paint");
    validate_paint_entries("first-paint");
    const post_click_lcp = await get_lcp_entries();
    assert_greater_than(post_click_lcp.length, pre_click_lcp.length,
      "Soft navigation should have triggered at least an LCP entry");
    assert_not_equals(post_click_lcp[post_click_lcp.length - 1].size,
      pre_click_lcp[pre_click_lcp.length - 1].size,
      "Soft navigation LCP element should not have identical size to the hard "
      + "navigation LCP element");
   }, test_name);
}

const click = button => {
  if (test_driver) {
    test_driver.click(button);
  }
}

const double_raf = () => {
  return new Promise(r => {
    requestAnimationFrame(()=>requestAnimationFrame(r));
  });
};

const setClickEvent = (t, button, push_state, add_content, push_url) => {
  button.addEventListener("click", async e => {
    // Jump through a task, to ensure task tracking is working properly.
    await new Promise(r => t.step_timeout(r, 0));

    // Fetch some content
    const response = await fetch("/soft-navigation-heuristics/resources/content.json");
    const json = await response.json();

    if (push_state) {
      // Change the URL
      if (push_url) {
        history.pushState({}, '', url + "?" + counter);
      } else {
        history.pushState({}, '');
      }
    }

    await add_content(json);
    ++counter;

    clicked = true;
  });
};

const validate_soft_navigation_entry = async (clicks, extra_validations, push_url) => {
  const [entries, options] = await new Promise(resolve => {
    (new PerformanceObserver((list, obs, options) => resolve(
      [list.getEntries(), options]))).observe(
      {type: 'soft-navigation', buffered: true});
    });
  const expected_clicks = Math.min(clicks, max_clicks);

  assert_equals(entries.length, expected_clicks,
                "Performance observer got an entry");
  assert_true(entries[0].name.includes(push_url ? url : document.location.href),
              "The soft navigation name is properly set");
  assert_not_equals(entries[0].navigationId,
                    performance.getEntriesByType("navigation")[0].navigationId,
                    "The navigation ID was incremented");
  assert_equals(performance.getEntriesByType("soft-navigation").length,
                expected_clicks, "Performance timeline got an entry");
  extra_validations(entries, options);

};

const validate_paint_entries = async type => {
  const entries = await new Promise(resolve => {
    (new PerformanceObserver(list => resolve(
      list.getEntriesByName(type)))).observe(
      {type: 'paint', buffered: true});
    });
  assert_equals(entries.length, 2, "There are two entries for " + type);
  assert_not_equals(entries[0].startTime, entries[1].startTime,
    "Entries have different timestamps for " + type);
};

const get_lcp_entries = async () => {
  const entries = await new Promise(resolve => {
    (new PerformanceObserver(list => resolve(
      list.getEntries()))).observe(
      {type: 'largest-contentful-paint', buffered: true});
    });
  return entries;
};

