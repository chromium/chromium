def assert_base_entry(entry,
                      level=None,
                      text=None,
                      time_start=None,
                      time_end=None,
                      stacktrace=None):
    assert "level" in entry
    assert isinstance(entry["level"], str)
    if level is not None:
        assert entry["level"] == level

    assert "text" in entry
    assert isinstance(entry["text"], str)
    if text is not None:
        assert entry["text"] == text

    assert "timestamp" in entry
    assert isinstance(entry["timestamp"], int)
    if time_start is not None:
        assert entry["timestamp"] >= time_start
    if time_end is not None:
        assert entry["timestamp"] <= time_end

    if stacktrace is not None:
        assert "stackTrace" in entry
        assert isinstance(entry["stackTrace"], list)


def assert_console_entry(entry,
                         method=None,
                         level=None,
                         text=None,
                         args=None,
                         time_start=None,
                         time_end=None,
                         realm=None,
                         stacktrace=None):
    assert_base_entry(entry, level, text, time_start, time_end, stacktrace)

    assert "type" in entry
    assert isinstance(entry["type"], str)
    assert entry["type"] == "console"

    assert "method" in entry
    assert isinstance(entry["method"], str)
    if method is not None:
        assert entry["method"] == method

    assert "args" in entry
    assert isinstance(entry["args"], list)
    if args is not None:
        assert entry["args"] == args

    if realm is not None:
        assert "realm" in entry
        assert isinstance(entry["realm"], str)


def assert_javascript_entry(entry,
                            level=None,
                            text=None,
                            time_start=None,
                            time_end=None,
                            stacktrace=None):
    assert_base_entry(entry, level, text, time_start, time_end, stacktrace)

    assert "type" in entry
    assert isinstance(entry["type"], str)
    assert entry["type"] == "javascript"
