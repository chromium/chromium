# Testing: https://w3c.github.io/aria/html-aam/#el-form

TEST_HTML = "<form id='target' aria-label='name'>content</form>"
TEST_HTML_NAMELESS = "<form id='target'>content</form>"

def test_atspi_named(atspi, session, inline):
    session.url = inline(TEST_HTML)

    # Spec:
    # Use WAI-ARIA mapping
    # If a form has no accessible name:
    # Role: ATK_ROLE_FORM

    node = atspi.find_node("target", session.url)
    assert atspi.Accessible.get_role(node) == atspi.Role.LANDMARK
    assert "xml-roles:form" in atspi.Accessible.get_attributes_as_array(node)

def test_atspi_nameless(atspi, session, inline):
    session.url = inline(TEST_HTML_NAMELESS)

    # Spec:
    # Use WAI-ARIA mapping
    # If a form has no accessible name:
    # Role: ATK_ROLE_FORM

    node = atspi.find_node("target", session.url)
    assert atspi.Accessible.get_role(node) == atspi.Role.FORM

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
