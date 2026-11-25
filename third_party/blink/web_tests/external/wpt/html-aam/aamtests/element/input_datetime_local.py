# Testing: https://w3c.github.io/aria/html-aam/#el-input-datetime-local

TEST_HTML = "<input id='target' type='datetime-local'>"

def test_atspi(atspi, session, inline):
    session.url = inline(TEST_HTML)

    # Spec:
    # Role: ATK_ROLE_CALENDAR

    node = atspi.find_node("target", session.url)
    assert atspi.Accessible.get_role(node) == atspi.Role.CALENDAR

# def test_axapi(axapi, session, inline):
#     session.url = inline(TEST_HTML)
#
#     # Spec:
#     # AXRole: AXTextField
#     # AXSubrole: (nil)
#     # AXRoleDescription: "text field"

# def test_ia2(ia2, session, inline):
#     session.url = inline(TEST_HTML)
#
#     # Spec:
#     # Role: IA2_ROLE_DATE_EDITOR

# def test_uia(uia, session, inline):
#     session.url = inline(TEST_HTML)
#
#     # Spec:
#     # Depends on UI design of implementation. The UI in Windows 10 Edge, for Example, is a composite of multiple spinners.
