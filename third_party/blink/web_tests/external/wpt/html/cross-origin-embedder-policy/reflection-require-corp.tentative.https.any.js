// META: global=window,dedicatedworker,sharedworker,serviceworker
test(t => assert_equals(crossOriginEmbedderPolicy, "require-corp"));
