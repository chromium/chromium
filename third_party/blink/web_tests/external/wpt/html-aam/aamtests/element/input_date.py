# Testing: https://w3c.github.io/aria/html-aam/#el-input-date

TEST_HTML = "<input id='target' type='date'>"

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
#     # AXRole: AXDateField
#     # AXSubrole: (nil)
#     # AXRoleDescription: "date field"

# def test_ia2(ia2, session, inline):
#     session.url = inline(TEST_HTML)
#
#     # Spec:
#     # If implemented as a textbox:
#     # Role: ROLE_SYSTEM_TEXT
#     # Object attributes: text-input-type:date
#     # If implemented as a date picker:
#     # Role: IA2_ROLE_DATE_EDITOR

# def test_uia(uia, session, inline):
#     session.url = inline(TEST_HTML)
#
#     # Spec:
#     # Depends on UI design of implementation. The UI in Windows 10 Edge, for example, is a composite of multiple spinners.
