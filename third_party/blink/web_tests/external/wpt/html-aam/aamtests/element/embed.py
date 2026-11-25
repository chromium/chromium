# Testing: https://w3c.github.io/aria/html-aam/#el-embed

TEST_HTML = "<embed id='target' type='image/png' src='/images/green.png'>"

def test_atspi(atspi, session, inline):
    session.url = inline(TEST_HTML)

    # Spec:
    # Role: ATK_ROLE_EMBEDDED

    node = atspi.find_node("target", session.url)
    assert atspi.Accessible.get_role(node) == atspi.Role.EMBEDDED

# def test_axapi(axapi, session, inline):
#     session.url = inline(TEST_HTML)
#
#     # Spec:
#     # Depends on format of data file

# def test_ia2(ia2, session, inline):
#     session.url = inline(TEST_HTML)
#
#     # Spec:
#     # Roles: ROLE_SYSTEM_CLIENT; IA2_ROLE_EMBEDDED_OBJECT
#     # States: STATE_SYSTEM_UNAVAILABLE for windowless plugin

# def test_uia(uia, session, inline):
#     session.url = inline(TEST_HTML)
#
#     # Spec:
#     # Control Type: Pane
