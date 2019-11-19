/**
 * A helper class that listens for accessibility notifications on the
 * |accessibilityController|, and signals that the test is complete
 * after receiving all of the specified expected notifications.
 */
class TestAccessibilityControllerNotificationListener {
  /**
   * @param {Test} test A Test object from web tests testharness.
   */
  constructor(test) {
    this.test = test;
    this.notification_queue_map = new Map();
    accessibilityController.addNotificationListener(
        this.test.step_func(this.onNotificationReceived, this));
  }

  /**
   * Enqueues an expected notification.
   * Note that events are queued into buckets organized by notification.
   * i.e. All "Focus" events are expected to fire in the order they are
   * enqueued, but interleaved notification types have no expectation
   * to be fired in a specific order.
   *
   * @param {string} notification The notification key.
   * @param {string} expected_name Expected |element.name| when the notification is received.
   * @param {string} expected_value_description Expected |element.valueDescription| when the notification is received.
   * @param {string} optional_key_down_key When specified, fires a |keyDown| event after receiving the notification using the specified key.
   */
  queueExpectedNotification(notification, expected_name, expected_value_description, optional_key_down_key) {
    var queue = this.notification_queue_map.get(notification);
    if (!queue) {
      queue = new Array();
      this.notification_queue_map.set(notification, queue);
    }
    queue.push({name: expected_name,
                value_description: expected_value_description,
                key_down_key: optional_key_down_key});
  }

  /**
   * Handles test expectations after receiving an accessibility notification.
   * When all expected notifications are received, signals the test is done.
   *
   * @param {Element} element The element this notification is for.
   * @param {string} notification The notification key.
   */
  onNotificationReceived(element, notification) {
    console.log(`Got ${notification} notification`);
    var queue = this.notification_queue_map.get(notification);
    if (!queue)
      return;

    var next_expectation = queue.shift();
    if (queue.length === 0) {
      this.notification_queue_map.delete(notification);
    }

    assert_equals(element.name.trim(), next_expectation.name);
    assert_equals(element.valueDescription.substr(20),
                  next_expectation.value_description);
    if (next_expectation.key_down_key) {
      eventSender.keyDown(next_expectation.key_down_key);
    }

    if (this.notification_queue_map.size === 0) {
      this.test.done();
    }
  }
}
