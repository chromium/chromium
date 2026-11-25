# Testing: https://w3c.github.io/aria/html-aam/#el-input-search

TEST_HTML = "<input id='target' type='search'>"

def test_atspi(atspi, session, inline):
    session.url = inline(TEST_HTML)

    # Spec:
    # Use WAI-ARIA mapping

    node = atspi.find_node("target", session.url)
    assert atspi.Accessible.get_role(node) == atspi.Role.ENTRY
    assert "xml-roles:searchbox" in atspi.Accessible.get_attributes_as_array(node)
    assert "text-input-type:search" in atspi.Accessible.get_attributes_as_array(node)
    assert atspi.Accessible.get_editable_text_iface(node) is not None

# def test_axapi(axapi, session, inline):
#     session.url = inline(TEST_HTML)
#
#     # Spec:
#     # Use WAI-ARIA mapping

# def test_ia2(ia2, session, inline):
#     session.url = inline(TEST_HTML)
#
#     # Spec:
#     # Use WAI-ARIA mapping

# def test_uia(uia, session, inline):
#     session.url = inline(TEST_HTML)
#
#     # Spec:
#     # Use WAI-ARIA mapping
