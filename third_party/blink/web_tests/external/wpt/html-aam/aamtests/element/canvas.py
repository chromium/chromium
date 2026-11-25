# Testing: https://w3c.github.io/aria/html-aam/#el-canvas

TEST_HTML = "<canvas id='target'></canvas>"

def test_atspi(atspi, session, inline):
    session.url = inline(TEST_HTML)

    # Spec:
    # Role: ATK_ROLE_CANVAS

    node = atspi.find_node("target", session.url)
    assert atspi.Accessible.get_role(node) == atspi.Role.CANVAS

# def test_axapi(axapi, session, inline):
#     session.url = inline(TEST_HTML)
#
#     # Spec:
#     # AXRole: AXGroup
#     # AXSubrole: (nil)
#     # AXRoleDescription: ""

# def test_ia2(ia2, session, inline):
#     session.url = inline(TEST_HTML)
#
#     # Spec:
#     # Roles: ROLE_SYSTEM_GRAPHIC; IA2_ROLE_CANVAS

# def test_uia(uia, session, inline):
#     session.url = inline(TEST_HTML)
#
#     # Spec:
#     # Control Type: Image
#     # Descendants of the canvas element are mapped separately.
