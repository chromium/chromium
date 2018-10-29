// Remove leading ...LayoutTests/ or ...web_tests/ .
function relativeTestPath(path) {
    return path.replace(/.*\/(LayoutTests|web_tests)\//, '');
}
