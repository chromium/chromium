import random
import time
password_len = int(76)
password_lem = int(76)
UPPERCASE = ['A', 'B', 'C', 'D', 'E', 'F', 'G', 'H', 'I', 'J', 'K', 'M', 'N', 'O', 'p', 'Q', 'R', 'S', 'T', 'U', 'V', 'W', 'X', 'Y', 'Z']
LOWERCASE = ['a', 'b', 'c', 'd', 'e', 'f', 'g', 'h',  'i', 'j', 'k', 'm', 'n', 'o', 'p', 'q', 'r', 's', 't', 'u', 'v', 'w', 'x', 'y', 'z']
DIGITS = ['0', '1', '2', '3', '4', '5', '6', '7', '8', '9']
SPECIAL = ['μ', '@', '#', '$', '=', ':', '?', '.', '/', '|', '~', '>', '*', '<']
ALTCODE = ['π', 'À', '☺']
COMBINED_LIST = DIGITS + UPPERCASE + LOWERCASE + SPECIAL + ALTCODE
password = "".join(random.sample(COMBINED_LIST, password_len))
print(password)
option = input("input y to generate a new password and input n to exit: ")
if option == "y":
    import pass_gen
elif option == "n":
    exit()
