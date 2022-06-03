function testElement(element, property, length) {
    var propertyValue = getComputedStyle(document.getElementById(element), '').getPropertyValue(property);

    if (propertyValue == "") {
        assert_equals(length, 0);
        return [];
    }

    var tracks = propertyValue.split(' ');
    assert_equals(tracks.length, length);
    return tracks;
}