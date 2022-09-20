var counter = 0;
var clicked;
const max_clicks = 50;
const url = "/foobar.html";
const test_soft_navigation = (add_content, button, push_state, clicks,
                              extra_validations, test_name) => {
  promise_test(async t => {
    setClickEvent(t, button, push_state, add_content);
    for (let i = 0; i < clicks; ++i) {
      clicked = false;
      click(button);
      await wait_for_click();
    }
    assert_equals(document.softNavigations, clicks);
    await validate_soft_navigation_entry(clicks, extra_validations);
   }, test_name);
}

const click = button => {
  if (test_driver) {
    test_driver.click(button);
  }
}

const wait_for_click = () => {
  return new Promise(r => {
    setInterval(() => {
      if(clicked) {
        r();
      }
    }, 10)
  });
}

const setClickEvent = (t, button, push_state, add_content) => {
  button.addEventListener("click", async e => {
    // Jump through a task, to ensure task tracking is working properly.
    await new Promise(r => t.step_timeout(r, 0));

    // Fetch some content
    const response = await fetch("../resources/content.json");
    const json = await response.json();

    if (push_state) {
      // Change the URL
      history.pushState({}, '', url + "?" + counter);
    }

    add_content(json);
    ++counter;

    clicked = true;
  });
};

const validate_soft_navigation_entry = async (clicks, extra_validations) => {
  const [entries, options] = await new Promise(resolve => {
    (new PerformanceObserver((list, obs, options) => resolve(
      [list.getEntries(), options]))).observe(
      {type: 'soft-navigation', buffered: true});
    });
  const expected_clicks = Math.min(clicks, max_clicks);

  assert_equals(entries.length, expected_clicks,
                "Performance observer got an entry");
  assert_true(entries[0].name.includes(url),
              "The soft navigation name is properly set");
  assert_not_equals(entries[0].navigationId,
                    performance.getEntriesByType("navigation")[0].navigationId,
                    "The navigation ID was incremented");
  assert_equals(performance.getEntriesByType("soft-navigation").length,
                expected_clicks, "Performance timeline got an entry");
  extra_validations(entries, options);
};
