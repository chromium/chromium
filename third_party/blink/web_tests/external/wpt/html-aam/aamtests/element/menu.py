# Testing: https://w3c.github.io/aria/html-aam/#el-menu

TEST_HTML = "<menu id='target'><li>item</li></menu>"

def test_atspi(atspi, session, inline):
    session.url = inline(TEST_HTML)

    # Spec:
    # Use WAI-ARIA mapping

    node = atspi.find_node("target", session.url)
    assert atspi.Accessible.get_role(node) == atspi.Role.LIST

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
